#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>
#include <omp.h>

#ifndef EXACT_MATH
#include "simd_math_prims.h"
#define expf        expapprox
#define logf        logapprox
#define powf(x,y)   expf(logf(x)*y)
#endif

#define PRIlink     PRIu16
#define SCNlink     SCNu16
#define PRItoken    PRIu32
#define SCNtoken    SCNu32

typedef uint16_t link_t;
typedef uint32_t token;

#define NULL_LINK   0xffffU

#define JUMP_ALPHA  0.5
#define FERT_ALPHA  0.5
#define LEX_ALPHA   0.001
#define NULL_ALPHA  0.001

#ifndef DOUBLE_PRECISION
#define COUNT_BITS  32
typedef float count;
#else
#define COUNT_BITS  64
typedef double count;
#endif

// size of the jump statistics array for the HMM model
#define JUMP_ARRAY_LEN  0x800
#define JUMP_SUM        (JUMP_ARRAY_LEN-1)
// estimated maximum jump (approximately, will be used for normalization only)
#define JUMP_MAX_EST    100.0

// size of the fertility statistics array (note that there is one such
// distribution per word type, so we need to be a bit strict about its size)
#define FERT_ARRAY_LEN  0x08

// maximum size of sentences (for fixed-size buffers)
#define MAX_SENT_LEN    0x400

#include "random.c"
#include "hash.c"

// these control the threshold of the map data structures (linear lookup vs
// hash table).
// Must be 0 or 2^n-1 (for some n >= 2), where 0 means that linear lookup is
// never used.
#define MAX_FIXED   0

#define MAKE_NAME(NAME) map_token_u32 ## NAME
#define INDEX_TYPE      size_t
#define KEY_TYPE        token
#define VALUE_TYPE      uint32_t
#define EMPTY_KEY       ((token)0xffffffffUL)
#define HASH_KEY        hash_u32_u32
#include "natmap.c"

#define MIN(x,y)    (((x)<(y))?(x):(y))
#define MAX(x,y)    (((x)>(y))?(x):(y))

struct sentence {
    link_t length;
    token tokens[];
};

struct text {
    char *filename;
    size_t n_sentences;
    token vocabulary_size;
    struct sentence **sentences;
};

struct text_alignment {
    int model;
    const struct text *source;
    const struct text *target;
    link_t **sentence_links;
    link_t *buf;
    struct map_token_u32 *source_prior;
    count *source_prior_sum;
    int has_jump_prior;
    count jump_prior[JUMP_ARRAY_LEN];
    count *fert_prior;
    struct map_token_u32 *source_count;
    count *inv_source_count_sum;
    count jump_counts[JUMP_ARRAY_LEN];
    count *fert_counts;
    // this number of sentences contain clean parallel data and should
    // contribute to the statistics (anything after this should still be
    // aligned, but don't trust the statistics):
    size_t n_clean; // 0 (the default) means all sentences should be used
    count null_prior;
};

double seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return 1e-9*(double)ts.tv_nsec + (double)ts.tv_sec;
}

void text_alignment_free(struct text_alignment *ta) {
    if (ta->source_prior != NULL) {
        for (size_t i=0; i<ta->source->vocabulary_size; i++)
            map_token_u32_clear(ta->source_prior + i);
        free(ta->source_prior);
        free(ta->source_prior_sum);
    }
    if (ta->fert_prior != NULL) {
        free(ta->fert_prior);
    }
    for (size_t i=0; i<ta->source->vocabulary_size; i++)
        map_token_u32_clear(ta->source_count + i);
    free(ta->source_count);
    free(ta->inv_source_count_sum);
    free(ta->sentence_links);
    free(ta->buf);
    free(ta->fert_counts);
    free(ta);
}

void text_alignment_write_moses(
        const struct text_alignment *ta, FILE *file, int reverse) {
    for (size_t sent=0; sent<ta->target->n_sentences; sent++) {
        if (ta->target->sentences[sent] == NULL ||
            ta->source->sentences[sent] == NULL) {
            fputc('\n', file);
        } else {
            size_t length = ta->target->sentences[sent]->length;
            const link_t *links = ta->sentence_links[sent];
            int first = 1;
            for (size_t j=0; j<length; j++) {
                if (links[j] != NULL_LINK) {
                    if (reverse) {
                        fprintf(file, first? "%d-%d": " %d-%d",
                                (int)j, (int)links[j]);
                    } else {
                        fprintf(file, first? "%d-%d": " %d-%d",
                                (int)links[j], (int)j);
                    }
                    first = 0;
                }
            }
            fputc('\n', file);
        }
    }
}

//void text_alignment_write_vocab(const struct text_alignment *ta, FILE *file) {
//    fprintf(file, "%u %u\n",
//            ta->source->vocabulary_size-1, ta->target->vocabulary_size-1);
//    for (size_t e=1; e<ta->source->vocabulary_size; e++) {
//        struct map_token_u32 *sc = ta->source_count + e;
//        token fs[sc->n_items];
//        uint32_t ns[sc->n_items];
//        map_token_u32_items(sc, fs, ns);
//        fprintf(file, "%zd", sc->n_items);
//        for (size_t i=0; i<sc->n_items; i++) {
//            if (fs[i] == 0) {
//                perror("text_alignment_write_vocab(): target type id == 0");
//                exit(1);
//            }
//            fprintf(file, " %"PRItoken" %"PRIu32, fs[i]-1, ns[i]);
//        }
//        fprintf(file, "\n");
//    }
//}

void text_alignment_write_stats(const struct text_alignment *ta, FILE *file) {
    // Total number of non-zero (e, f) pairs
    //size_t n_priors = 0;

    //for (size_t e=0; e<ta->source->vocabulary_size; e++)
    //    n_priors += ta->source_count[e].n_items;

    //fprintf(file, "%"PRItoken" %"PRItoken" %zd\n",
    //        ta->source->vocabulary_size, ta->target->vocabulary_size,
    //        n_priors);

    //for (token e=0; e<ta->source->vocabulary_size; e++) {
    //    size_t k = ta->source_count[e].n_items;
    //    token fs[k];
    //    uint32_t ns[k];
    //    map_token_u32_items(ta->source_count + e, fs, ns);
    //    for (size_t i=0; i<k; i++)
    //        fprintf(file, "%"PRItoken" %"PRItoken" %"PRIu32"\n",
    //                e, fs[i], ns[i]);
    //}

    fprintf(file, "%d\n", JUMP_ARRAY_LEN);
    for (size_t i=0; i<JUMP_ARRAY_LEN; i++) {
        fprintf(file, "%d\n", (int)roundf(ta->jump_counts[i]-JUMP_ALPHA));
    }
}

// Get the index of the jump distribution parameter vector for a jump from
// position i to j (in a sentence with total length len)
inline static size_t get_jump_index(int i, int j, int len) {
    return (size_t)MAX(0, MIN(JUMP_ARRAY_LEN-1, j - i + JUMP_ARRAY_LEN/2));
}

inline static size_t get_fert_index(size_t e, int fert) {
    return e*FERT_ARRAY_LEN + (size_t)MIN(fert, FERT_ARRAY_LEN-1);
}

void text_alignment_sample(
        struct text_alignment *ta, random_state *state,
        count *sentence_scores, struct text_alignment **tas,
        int n_samplers) {
    const int n_samples = 1;
    const int argmax = tas != NULL;
    const int model = ta->model;
    struct sentence **source_sentences = ta->source->sentences;
    struct sentence **target_sentences = ta->target->sentences;
    // probability distribution to sample from
    count ps[MAX_SENT_LEN+1];
    // fertility of tokens in sentence
    int fert[MAX_SENT_LEN];
    count *jump_counts = ta->jump_counts;
    count *fert_counts = ta->fert_counts;
    const size_t n_sentences =
        ta->n_clean? ta->n_clean: ta->target->n_sentences;

    // the fertility distributions (unlike the jump and lexical distributions)
    // are sampled explicitly, and the categorical distributions are fixed
    // throughout the iteration.
    if (model >= 3) {
        size_t *e_count = malloc(sizeof(size_t)*ta->source->vocabulary_size);
        if (e_count == NULL) {
            perror("text_alignment_sample(): unable to allocate e_count");
            exit(1);
        }

        for (size_t i=0; i<ta->source->vocabulary_size; i++)
            e_count[i] = 0;

        if (ta->fert_prior != NULL) {
            // TODO: decide on whether to add FERT_ALPHA
            // TODO: 0 or 1-based? NULL included?
            for (size_t i=0; i<FERT_ARRAY_LEN*ta->source->vocabulary_size; i++)
            {
                fert_counts[i] = ta->fert_prior[i] + (count) FERT_ALPHA;
            }
        } else {
            for (size_t i=0; i<FERT_ARRAY_LEN*ta->source->vocabulary_size; i++)
                fert_counts[i] = (count) FERT_ALPHA;
        }

        // go through the text and compute fertility statistics
        for (size_t sent=0; sent<n_sentences; sent++) {
            link_t *links = ta->sentence_links[sent];
            // in case this sentence pair should not be aligned, skip it
            if (links == NULL) continue;
            const struct sentence *source_sentence = source_sentences[sent];
            const struct sentence *target_sentence = target_sentences[sent];
            const size_t source_length = source_sentence->length;
            const size_t target_length = target_sentence->length;
            const token *source_tokens = source_sentence->tokens;

            for (size_t i=0; i<source_length; i++)
                fert[i] = 0;
            for (size_t j=0; j<target_length; j++)
                if (links[j] != NULL_LINK)
                    fert[links[j]]++;
            for (size_t i=0; i<source_length; i++) {
                e_count[source_tokens[i]]++;
                fert_counts[get_fert_index(source_tokens[i], fert[i])] += 1.0;
            }
        }

        // sample a categorical fertility distribution from the posterior
        // for each source word e.
        //
        // since we only ever want to use
        //      P(phi(i)) / P(phi(i)-1)
        // position i directly stores this value.
        // Index 0 is undefined, and the maximum value contains a very
        // low probability (because it should never be used).
        for (token e=1; e<ta->source->vocabulary_size; e++) {
            // skip vocabulary items that do not actually occur in this text
            if (e_count[e] == 0) continue;
            count alpha[FERT_ARRAY_LEN];
            count *buf = fert_counts + get_fert_index(e, 0);
            memcpy(alpha, buf, FERT_ARRAY_LEN*sizeof(count));
#if COUNT_BITS == 32
            random_dirichlet32_unnormalized(state, FERT_ARRAY_LEN, alpha, buf);
#else
            random_dirichlet64_unnormalized(state, FERT_ARRAY_LEN, alpha, buf);
#endif
            buf[FERT_ARRAY_LEN-1] = (count) 1e-10;
            for (size_t i=FERT_ARRAY_LEN-2; i; i--)
                buf[i] /= buf[i-1];
        }

        free(e_count);
    }

    count *acc_ps = NULL;
    if (argmax) acc_ps = malloc(MAX_SENT_LEN*(MAX_SENT_LEN+1)*sizeof(count));
    // aa_jp1_table[j] will contain the alignment of the nearest non-NULL
    // aligned word to the right (or source_sentence->length if there is no
    // such word)
    int aa_jp1_table[MAX_SENT_LEN];
    int aa_jp1;
    for (size_t sent=0; sent<ta->target->n_sentences; sent++) {
        link_t *links = ta->sentence_links[sent];
        // in case this sentence pair should not be aligned, skip it
        if (links == NULL) continue;
        const struct sentence *source_sentence = source_sentences[sent];
        const struct sentence *target_sentence = target_sentences[sent];
        const size_t source_length = source_sentence->length;
        const size_t target_length = target_sentence->length;
        const token *source_tokens = source_sentence->tokens;
        const token *target_tokens = target_sentence->tokens;

        int samples_left = n_samples-1;
        int samplers_left = n_samplers-1;

        if (argmax) {
            for (size_t k=0; k<target_length*(source_length+1); k++)
                acc_ps[k] = (count) 0.0;
        }

        // This is the head of a loop (look for gotos below) which iterates
        // n_samples * n_samplers times, accumulating distributions from the
        // independent samplers.
resample:;
        if (tas != NULL) ta = tas[samplers_left];
        size_t acc_base = 0;
        links = ta->sentence_links[sent];

        // if HMM model is used:
        if (model >= 2) {
            // initialize table of nearest non-NULL alignment to the right
            aa_jp1 = source_length;
            for (size_t j=target_length; j>0; j--) {
                aa_jp1_table[j-1] = aa_jp1;
                if (links[j-1] != NULL_LINK) aa_jp1 = links[j-1];
            }
        }
        // if fertility model is used:
        if (model >= 3) {
            // compute fertilities of the tokens in this sentence
            for (size_t i=0; i<source_length; i++)
                fert[i] = 0;
            for (size_t j=0; j<target_length; j++)
                if (links[j] != NULL_LINK)
                    fert[links[j]]++;
        }

        // aa_jm1 will always contain the alignment of the nearest non-NULL
        // aligned word to the left (or -1 if there is no such word)
        int aa_jm1 = -1;
        for (size_t j=0; j<target_length; j++) {
            const token f = target_tokens[j];
            const link_t old_i = links[j];
            token old_e;

            aa_jp1 = aa_jp1_table[j];

            if(old_i == NULL_LINK) {
                old_e = 0;
            } else {
                old_e = source_tokens[old_i];
                if (model >= 3)
                    fert[old_i]--;
            }

            uint32_t reduced_count = 0;
            if (sent < n_sentences) {
                ta->inv_source_count_sum[old_e] =
                      (count)1.0
                    / ((count)1.0/ta->inv_source_count_sum[old_e] -
                            (count)1.0);
                reduced_count =
                    map_token_u32_add(ta->source_count + old_e, f, -1);
                if (reduced_count & 0x80000000UL) {
                    fprintf(stderr,
                        "old_e = %"PRItoken", n_items = %zd, dynamic = %u\n",
                        old_e, (size_t)ta->source_count[old_e].n_items,
                        map_token_u32_is_dynamic(ta->source_count + old_e));
                }
                assert ((reduced_count & 0x80000000UL) == 0);
            }

            size_t skip_jump = 0;

            if (model >= 2) {
                skip_jump = get_jump_index(
                        aa_jm1, aa_jp1, source_length);
            }

            if (model >= 2 && sent < n_sentences) {
                if (links[j] == NULL_LINK) {
                    // if this target token is NULL aligned, only one jump
                    // needs to be removed from the statistics:
                    jump_counts[JUMP_SUM] -= (count) 1.0;
                    jump_counts[skip_jump] -= (count) 1.0;
                } else {
                    // otherwise, there are two jumps:
                    const size_t old_jump1 = get_jump_index(
                            aa_jm1, links[j], source_length);
                    const size_t old_jump2 = get_jump_index(
                            links[j], aa_jp1, source_length);
                    jump_counts[JUMP_SUM] -= (count) 2.0;
                    jump_counts[old_jump1] -= (count) 1.0;
                    jump_counts[old_jump2] -= (count) 1.0;
                }
            }

            count ps_sum = 0.0;
            uint32_t null_n = 0;
            map_token_u32_get(ta->source_count + 0, f, &null_n);
            // for speed, we use separate versions of the inner loop depending
            // on the model used (in spite of the code redundancy)
            if (model >= 3) {
                size_t jump1 = get_jump_index(
                        aa_jm1, 0, source_length);
                size_t jump2 = get_jump_index(
                        0, aa_jp1, source_length);
                for (size_t i=0; i<source_length; i++) {
                    const token e = source_tokens[i];
                    const size_t fert_idx = get_fert_index(e, fert[i]+1);
                    uint32_t n = 0;
                    map_token_u32_get(ta->source_count + e, f, &n);
                    if (ta->source_prior != NULL) {
                        float alpha = 0.0f;
                        map_token_u32_get(ta->source_prior + e, f,
                                          (uint32_t*)&alpha);
                        alpha += LEX_ALPHA;
                        ps_sum += ta->inv_source_count_sum[e] *
                                  ((count)alpha + (count)n) *
                                  jump_counts[jump1] * jump_counts[jump2] *
                                  fert_counts[fert_idx];
                    } else {
                        ps_sum += ta->inv_source_count_sum[e] *
                                  (LEX_ALPHA + (count)n) *
                                  jump_counts[jump1] * jump_counts[jump2] *
                                  fert_counts[fert_idx];
                    }
                    ps[i] = ps_sum;
                    // We can same a few cycles by replacing calls to
                    // get_jump_index() with bounded increment/decrement
                    // operations of the jump length distribution indexes
                    jump1 = MIN(JUMP_ARRAY_LEN-1, jump1+1);
                    jump2 = MAX(0, jump2-1);
                }
                if (sentence_scores != NULL) {
                    count max_p = 0.0;
                    for (size_t i=0; i<source_length; i++) {
                        const count p = ps[i] - (i? ps[i-1]: 0.0);
                        if (p > max_p) max_p = p;
                    }
                    sentence_scores[sent] += logf(
                            max_p / ((count)jump_counts[JUMP_SUM]*
                                     (count)jump_counts[JUMP_SUM]));
                }
                // rather than scaling the non-NULL probabilities with Z^-2
                // for the jump distribution normalization factor Z, we scale
                // the NULL probability with Z^1 instead, since the sampling
                // distribution will be normalized anyway.
                // Beware of this if you ever make modifications here!
                ps_sum += ta->null_prior * ta->inv_source_count_sum[0] *
                          (NULL_ALPHA +(count)null_n) *
                          jump_counts[JUMP_SUM] * jump_counts[skip_jump];
            } else if (model >= 2) {
                size_t jump1 = get_jump_index(
                        aa_jm1, 0, source_length);
                size_t jump2 = get_jump_index(
                        0, aa_jp1, source_length);
                for (size_t i=0; i<source_length; i++) {
                    const token e = source_tokens[i];
                    uint32_t n = 0;
                    map_token_u32_get(ta->source_count + e, f, &n);
                    if (ta->source_prior != NULL) {
                        float alpha = 0.0f;
                        map_token_u32_get(ta->source_prior + e, f,
                                          (uint32_t*)&alpha);
                        alpha += LEX_ALPHA;
                        ps_sum += ta->inv_source_count_sum[e] *
                                  (alpha + (count)n) *
                                  jump_counts[jump1] * jump_counts[jump2];
                    } else {
                        ps_sum += ta->inv_source_count_sum[e] *
                                  (LEX_ALPHA + (count)n) *
                                  jump_counts[jump1] * jump_counts[jump2];
                    }
                    ps[i] = ps_sum;
                    // We can same a few cycles by replacing calls to
                    // get_jump_index() with bounded increment/decrement
                    // operations of the jump length distribution indexes
                    jump1 = MIN(JUMP_ARRAY_LEN-1, jump1+1);
                    jump2 = MAX(0, jump2-1);
                }
                if (sentence_scores != NULL) {
                    count max_p = 0.0;
                    for (size_t i=0; i<source_length; i++) {
                        const count p = ps[i] - (i? ps[i-1]: 0.0);
                        if (p > max_p) max_p = p;
                    }
                    sentence_scores[sent] += logf(
                            max_p / ((count)jump_counts[JUMP_SUM]*
                                     (count)jump_counts[JUMP_SUM]));
                }
                // rather than scaling the non-NULL probabilities with Z^-2
                // for the jump distribution normalization factor Z, we scale
                // the NULL probability with Z^1 instead, since the sampling
                // distribution will be normalized anyway.
                // Beware of this if you ever make modifications here!
                ps_sum += ta->null_prior * ta->inv_source_count_sum[0] *
                          (NULL_ALPHA + (count)null_n) *
                          jump_counts[JUMP_SUM] * jump_counts[skip_jump];
            } else {
                for (size_t i=0; i<source_length; i++) {
                    const token e = source_tokens[i];
                    uint32_t n = 0;
                    map_token_u32_get(ta->source_count + e, f, &n);
                    if (ta->source_prior != NULL) {
                        float alpha = 0.0f;
                        map_token_u32_get(ta->source_prior + e, f,
                                          (uint32_t*)&alpha);
                        alpha += LEX_ALPHA;
                        ps_sum += ta->inv_source_count_sum[e] *
                                  (alpha + (count)n);
                    } else {
                        ps_sum += ta->inv_source_count_sum[e] *
                                  (LEX_ALPHA + (count)n);
                    }
                    ps[i] = ps_sum;
                }
                if (sentence_scores != NULL) {
                    count max_p = 0.0;
                    for (size_t i=0; i<source_length; i++) {
                        const count p = ps[i] - (i? ps[i-1]: 0.0);
                        if (p > max_p) max_p = p;
                    }
                    sentence_scores[sent] += logf(max_p);
                }
                ps_sum += ta->null_prior * ta->inv_source_count_sum[0] *
                          (NULL_ALPHA + (count)null_n);
            }
            ps[source_length] = ps_sum;

            if (argmax) {
                count scale = (count)1.0 / ps_sum;
                acc_ps[acc_base] += ps[0] * scale;
                for (size_t i=1; i<source_length+1; i++)
                    acc_ps[acc_base+i] += (ps[i] - ps[i-1]) * scale;
                acc_base += source_length+1;
            }

            link_t new_i;
            if (sentence_scores == NULL) {
                if ((!argmax) || samples_left) {
                    // normal case: simply from distribution
                    new_i = random_unnormalized_cumulative_categorical32(
                            state, ps, source_length+1);
                } else {
                    // if we have collected enough samples, do argmax over
                    // the accumulated probabilities
                    new_i = 0;
                    acc_base -= source_length+1;
                    count best_p = acc_ps[acc_base + 0];
                    for (size_t i=1; i<source_length+1; i++) {
                        const count p = acc_ps[acc_base + i];
                        if (p > best_p) {
                            new_i = i;
                            best_p = p;
                        }
                    }
                    acc_base += source_length+1;
                }
            } else {
                // if we are just calculating scores, don't sample at all
                // this could have been a no-op, but old_i is NULL_LINK in
                // case of unaligned words, whereas new_i is assumed to be one
                // index beyond the sentence (== source_length)
                new_i = (old_i == NULL_LINK)? source_length : old_i;
            }
            token new_e;
            if (new_i == source_length) {
                new_e = 0;
                links[j] = NULL_LINK;
            } else {
                new_e = source_tokens[new_i];
                links[j] = new_i;
                if (model >= 3)
                    fert[new_i]++;
            }

            if (sent < n_sentences) {
                if (old_e != new_e && reduced_count == 0) {
                    // If we reduced the old count to zero and we sampled a
                    // link to a different source token, remove the old zero
                    // count in order to save space.
                    int r = map_token_u32_delete(ta->source_count + old_e, f);
                    assert (r);
                }
                ta->inv_source_count_sum[new_e] =
                      (count)1.0
                    / ((count)1.0/ta->inv_source_count_sum[new_e] + (count)1.0);
                map_token_u32_add(ta->source_count + new_e, f, 1);
            }

            if (sent < n_sentences && model >= 2) {
                if (new_e == 0) {
                    jump_counts[JUMP_SUM] += (count) 1.0;
                    jump_counts[skip_jump] += (count) 1.0;
                } else {
                    const size_t new_jump1 = get_jump_index(
                            aa_jm1, new_i, source_length);
                    const size_t new_jump2 = get_jump_index(
                            new_i, aa_jp1, source_length);
                    jump_counts[JUMP_SUM] += (count) 2.0;
                    jump_counts[new_jump1] += (count) 1.0;
                    jump_counts[new_jump2] += (count) 1.0;
                }
            }
            if (model >= 2 && new_e != 0)
                aa_jm1 = new_i;
        }
        if (sentence_scores != NULL)
            sentence_scores[sent] /= (count)target_length;

        if (argmax) {
            if (samplers_left) {
                samplers_left--;
                goto resample;
            } else if (samples_left) {
                samplers_left = n_samplers-1;
                samples_left--;
                goto resample;
            }
        }
    }
    if (argmax) free(acc_ps);
}

void text_alignment_make_counts(struct text_alignment *ta) {
    const int model = ta->model;
    struct sentence **source_sentences = ta->source->sentences;
    struct sentence **target_sentences = ta->target->sentences;

    // TODO: do we need a special case for NULL links? Probably not, since
    //       NULL alignment statistics could also provide valuable prior
    //       information
    //map_token_u32_reset(ta->source_count + 0);
    //ta->inv_source_count_sum[0] =
    //    NULL_ALPHA * (count)ta->target->vocabulary_size;

    for (size_t i=0; i<ta->source->vocabulary_size; i++) {
        map_token_u32_reset(ta->source_count + i);
        if (ta->source_prior != NULL) {
            ta->inv_source_count_sum[i] = ta->source_prior_sum[i];
        } else {
            ta->inv_source_count_sum[i] =
                LEX_ALPHA * (count)ta->target->vocabulary_size;
        }
    }
    if (model >= 2) {
        if (ta->has_jump_prior) {
            // TODO: decide on whether to add JUMP_ALPHA
            ta->jump_counts[JUMP_SUM] = (count) (JUMP_MAX_EST*JUMP_ALPHA);
            for (size_t i=0; i<JUMP_ARRAY_LEN-1; i++) {
                ta->jump_counts[i] = ta->jump_prior[i] + (count) JUMP_ALPHA;
                ta->jump_counts[JUMP_SUM] += ta->jump_prior[i];
            }
        } else {
            for (size_t i=0; i<JUMP_ARRAY_LEN-1; i++)
                ta->jump_counts[i] = (count) JUMP_ALPHA;
            ta->jump_counts[JUMP_SUM] = (count) (JUMP_MAX_EST*JUMP_ALPHA);
        }
    }
    const size_t n_sentences =
        ta->n_clean? ta->n_clean: ta->target->n_sentences;
    for (size_t sent=0; sent<n_sentences; sent++) {
        link_t *links = ta->sentence_links[sent];
        if (links == NULL) continue;
        const struct sentence *source_sentence = source_sentences[sent];
        const struct sentence *target_sentence = target_sentences[sent];
        const size_t source_length = source_sentence->length;
        const size_t target_length = target_sentence->length;
        int aa_jm1 = -1;
        for (size_t j=0; j<target_length; j++) {
            const link_t i = links[j];
            const token e = (i == NULL_LINK)? 0 : source_sentence->tokens[i];
            const token f = target_sentence->tokens[j];
            ta->inv_source_count_sum[e] += (count)1.0;
            map_token_u32_add(ta->source_count + e, f, 1);
            if (model >= 2 && e != 0) {
                const size_t jump = get_jump_index(aa_jm1, i, source_length);
                aa_jm1 = i;
                ta->jump_counts[jump] += (count)1.0;
                ta->jump_counts[JUMP_SUM] += (count)1.0;
            }
        }
        if (model >= 2 && aa_jm1 >= 0) {
            ta->jump_counts[get_jump_index(
                    aa_jm1, source_length, source_length)] += (count) 1.0;
            ta->jump_counts[JUMP_SUM] += (count) 1.0;
        }
    }
    for (size_t i=0; i<ta->source->vocabulary_size; i++)
        ta->inv_source_count_sum[i] = (count)1.0 / ta->inv_source_count_sum[i];
}

void text_alignment_randomize(struct text_alignment *ta, random_state *state) {
    struct sentence **source_sentences = ta->source->sentences;
    struct sentence **target_sentences = ta->target->sentences;
    for (size_t sent=0; sent<ta->target->n_sentences; sent++) {
        link_t *links = ta->sentence_links[sent];
        if (links == NULL) continue;
        const struct sentence *source_sentence = source_sentences[sent];
        const struct sentence *target_sentence = target_sentences[sent];
        for (size_t j=0; j<target_sentence->length; j++) {
            if (random_uniform32(state) < ta->null_prior) {
                links[j] = NULL_LINK;
            } else {
                links[j] = random_uint32_biased(state, source_sentence->length);
            }
        }
    }
}

int text_alignment_load_priors(
        struct text_alignment *ta, const char *filename, int reverse)
{
    size_t lineno = 1;
    FILE *file = (!strcmp(filename, "-"))? stdin: fopen(filename, "r");
    if (file == NULL) {
        perror("text_alignment_load_priors(): failed to open text file");
        return -1;
    }
 
    size_t source_vocabulary_size, target_vocabulary_size, n_lex_priors,
           n_jump_priors, n_fert_priors,
           n_fwd_jump_priors, n_fwd_fert_priors,
           n_rev_jump_priors, n_rev_fert_priors;
    if (fscanf(file, "%zd %zd %zd %zd %zd %zd %zd\n",
                &source_vocabulary_size,
                &target_vocabulary_size,
                &n_lex_priors,
                &n_fwd_jump_priors,
                &n_rev_jump_priors,
                &n_fwd_fert_priors,
                &n_rev_fert_priors) != 7)
    {
        fprintf(stderr,
                "text_alignment_load_priors(): failed to read header in %s\n",
                filename);
        if (file != stdin) fclose(file);
        return -1;
    }
    lineno++;

    if (reverse) {
        n_jump_priors = n_rev_jump_priors;
        n_fert_priors = n_rev_fert_priors;
    } else {
        n_jump_priors = n_fwd_jump_priors;
        n_fert_priors = n_fwd_fert_priors;
    }

    if (n_lex_priors) {
        if ((ta->source_prior =
             malloc(ta->source->vocabulary_size*sizeof(struct map_token_u32))
            ) == NULL)
        {
            perror("text_alignment_load_priors(): "
                   "failed to allocate buffer pointers");
            exit(EXIT_FAILURE);
        }
        for (size_t i=0; i<ta->source->vocabulary_size; i++)
            map_token_u32_create(ta->source_prior + i);
        if ((ta->source_prior_sum =
             malloc(sizeof(count)*ta->source->vocabulary_size)) == NULL)
        {
            perror("text_alignment_load_priors(): "
                   "failed to allocate counter array");
            exit(EXIT_FAILURE);
        }
        for (size_t i=0; i<ta->source->vocabulary_size; i++) {
            ta->source_prior_sum[i] = 0.0;
        }
    }

    if (n_fert_priors) {
        if ((ta->fert_prior =
             malloc(ta->source->vocabulary_size*sizeof(count)*FERT_ARRAY_LEN))
                == NULL)
        {
            perror("text_alignment_create(): failed to allocate fertility "
                   "prior array");
            exit(EXIT_FAILURE);
        }
        for (size_t i=0;i<ta->source->vocabulary_size*FERT_ARRAY_LEN;i++)
            ta->fert_prior[i] = 0.0;
    }

    // TODO: check that JUMP_ALPHA and FERT_ALPHA are added where they should,
    // otherwise add here! (above and below)
    if (n_jump_priors) {
        ta->has_jump_prior = 1;

        for (size_t i=0; i<JUMP_ARRAY_LEN; i++)
            ta->jump_prior[i] = 0.0;
    }

    size_t t;
    if (reverse) {
        t = source_vocabulary_size;
        source_vocabulary_size = target_vocabulary_size;
        target_vocabulary_size = t;
    }

    if (source_vocabulary_size != ta->source->vocabulary_size ||
        target_vocabulary_size != ta->target->vocabulary_size)
    {
        fprintf(stderr,
                "text_alignment_load_priors(): vocabulary size mismatch, "
                "source is %zd (expected %"PRItoken") "
                "and target is %zd (expected %"PRItoken") in %s\n",
                source_vocabulary_size, ta->source->vocabulary_size,
                target_vocabulary_size, ta->target->vocabulary_size,
                filename);
        if (file != stdin) fclose(file);
        return -1;
    }

    for (size_t i=0; i<n_lex_priors; i++) {
        token e, f, t;
        float alpha;
        if (fscanf(file, "%"SCNtoken" %"SCNtoken" %f\n", &e, &f, &alpha) != 3) {
            fprintf(stderr,
                    "text_alignment_load_priors(): error in line %zd of %s\n",
                    i+2, filename);
            if (file != stdin) fclose(file);
            return -1;
        }
        lineno++;
        if (reverse) {
            t = e;
            e = f;
            f = t;
        }
        // TODO: fix this properly
        map_token_u32_add(ta->source_prior + e, f, *((uint32_t*)&alpha));
        ta->source_prior_sum[e] += alpha;
    }

    if (n_lex_priors) {
        for (size_t e=0; e<ta->source->vocabulary_size; e++) {
            ta->source_prior_sum[e] +=
                LEX_ALPHA * (float)ta->target->vocabulary_size;
        }
    }

    for (size_t i=0; i<n_fwd_jump_priors; i++) {
        int jump;
        size_t jump_index;
        float alpha;
        if (fscanf(file, "%d %f\n", &jump, &alpha) != 2) {
            fprintf(stderr,
                    "text_alignment_load_priors(): error in line %zd of %s\n",
                    lineno, filename);
            if (file != stdin) fclose(file);
            return -1;
        }
        jump_index = MAX(0, MIN(JUMP_ARRAY_LEN-1, jump + JUMP_ARRAY_LEN/2));
        if (! reverse)
            ta->jump_prior[jump_index] += alpha;
        lineno++;
    }

    for (size_t i=0; i<n_rev_jump_priors; i++) {
        int jump;
        size_t jump_index;
        float alpha;
        if (fscanf(file, "%d %f\n", &jump, &alpha) != 2) {
            fprintf(stderr,
                    "text_alignment_load_priors(): error in line %zd of %s\n",
                    i+2, filename);
            if (file != stdin) fclose(file);
            return -1;
        }
        jump_index = MAX(0, MIN(JUMP_ARRAY_LEN-1, jump + JUMP_ARRAY_LEN/2));
        if (reverse)
            ta->jump_prior[jump_index] += alpha;
        lineno++;
    }

    for (size_t i=0; i<n_fwd_fert_priors; i++) {
        int k;
        token e;
        size_t fert_index;
        float alpha;
        if (fscanf(file, "%"SCNtoken" %d %f\n", &e, &k, &alpha) != 3) {
            fprintf(stderr,
                    "text_alignment_load_priors(): error in line %zd of %s\n",
                    lineno, filename);
            if (file != stdin) fclose(file);
            return -1;
        }
        fert_index = get_fert_index(e, k);
        if (! reverse) {
            if (e >= ta->source->vocabulary_size) {
                fprintf(stderr,
                        "text_alignment_load_priors(): index %"PRItoken" out"
                        " of range (forward)", e);
                if (file != stdin) fclose(file);
                return -1;
            }
            ta->fert_prior[fert_index] += alpha;
        }
        lineno++;
    }

    for (size_t i=0; i<n_rev_fert_priors; i++) {
        int k;
        token e;
        size_t fert_index;
        float alpha;
        if (fscanf(file, "%"SCNtoken" %d %f\n", &e, &k, &alpha) != 3) {
            fprintf(stderr,
                    "text_alignment_load_priors(): error in line %zd of %s\n",
                    lineno, filename);
            if (file != stdin) fclose(file);
            return -1;
        }
        fert_index = get_fert_index(e, k);
        if (reverse) {
            if (e >= ta->source->vocabulary_size) {
                fprintf(stderr,
                        "text_alignment_load_priors(): index %"PRItoken" out"
                        " of range (reverse)", e);
                if (file != stdin) fclose(file);
                return -1;
            }
            ta->fert_prior[fert_index] += alpha;
        }
        lineno++;
    }

    return 0;
}

struct text_alignment *text_alignment_create(
        const struct text *source, const struct text *target)
{
    if (source->n_sentences != target->n_sentences) {
        fprintf(stderr, "text_alignment_create(): number of sentences "
                        "differ in texts!\n");
        return NULL;
    }
    struct text_alignment *ta;
    if ((ta = malloc(sizeof(*ta))) == NULL) {
        perror("text_alignment_create(): failed to allocate structure");
        exit(EXIT_FAILURE);
    }
    ta->model = 1;
    ta->source = source;
    ta->target = target;
    ta->n_clean = 0;

    // These should be initialized with text_alignment_load_priors()
    ta->source_prior = NULL;
    ta->source_prior_sum = NULL;
    ta->fert_prior = NULL;
    ta->has_jump_prior = 0;

    size_t buf_size = 0;
    for (size_t i=0; i<target->n_sentences; i++) {
        if (target->sentences[i] != NULL && source->sentences[i] != NULL)
            buf_size += (size_t)target->sentences[i]->length;
    }
    if ((ta->buf = malloc(buf_size*sizeof(link_t))) == NULL) {
        perror("text_alignment_create(): failed to allocate buffer");
        exit(EXIT_FAILURE);
    }
    if ((ta->sentence_links = malloc(target->n_sentences*sizeof(link_t*)))
            == NULL)
    {
        perror("text_alignment_create(): failed to allocate buffer pointers");
        exit(EXIT_FAILURE);
    }
    link_t *ptr = ta->buf;
    for (size_t i=0; i<target->n_sentences; i++) {
        if (target->sentences[i] != NULL && source->sentences[i] != NULL)
        {
            ta->sentence_links[i] = ptr;
            ptr += target->sentences[i]->length;
        } else {
            ta->sentence_links[i] = NULL;
        }
    }
    if ((ta->source_count =
         malloc(source->vocabulary_size*sizeof(struct map_token_u32))
        ) == NULL)
    {
        perror("text_alignment_create(): failed to allocate buffer pointers");
        exit(EXIT_FAILURE);
    }
    for (size_t i=0; i<source->vocabulary_size; i++)
        map_token_u32_create(ta->source_count + i);
    if ((ta->inv_source_count_sum =
         malloc(sizeof(count)*source->vocabulary_size)) == NULL)
    {
        perror("text_alignment_create(): failed to allocate counter array");
        exit(EXIT_FAILURE);
    }
    if ((ta->fert_counts =
         malloc(source->vocabulary_size*sizeof(count)*FERT_ARRAY_LEN))
            == NULL)
    {
        perror("text_alignment_create(): failed to allocate fertility counts");
        exit(EXIT_FAILURE);
    }

    return ta;
}

void sentence_free(struct sentence *sentence) {
    free(sentence);
}

struct sentence *sentence_read(FILE *file, token vocabulary_size) {
    link_t length;
    if (fscanf(file, "%"SCNlink, &length) != 1) {
        perror("sentence_read(): failed to read sentence length");
        exit(EXIT_FAILURE);
    }
    if (length == 0) return NULL;
    if (length > MAX_SENT_LEN) {
        perror("sentence_read(): sentence too long");
        exit(EXIT_FAILURE);
    }
    struct sentence *sentence;
    sentence = malloc(sizeof(struct sentence) + length*sizeof(token));
    if (sentence == NULL) {
        perror("sentence_read(): failed to allocate structure");
        exit(EXIT_FAILURE);
    }
    sentence->length = length;
    for (link_t i=0; i<length; i++) {
        if (fscanf(file, "%"SCNtoken, &(sentence->tokens[i])) != 1) {
            perror("sentence_read(): failed to read token");
            exit(EXIT_FAILURE);
        }
        sentence->tokens[i]++;
        if (sentence->tokens[i] >= vocabulary_size) {
            fprintf(stderr, "sentence_read(): vocabulary size is %"PRItoken
                            " but found token %"PRItoken"\n",
                            vocabulary_size, sentence->tokens[i]);
            exit(EXIT_FAILURE);
        }
    }
    return sentence;
}

void text_free(struct text *text) {
    for (size_t i=0; i<text->n_sentences; i++)
        if (text->sentences[i] != NULL) sentence_free(text->sentences[i]);
    free(text->sentences);
    free(text);
}

void text_write(struct text *text, FILE *file) {
    fprintf(file, "%zd %"PRItoken"\n",
            text->n_sentences, text->vocabulary_size);
    for (size_t i=0; i<text->n_sentences; i++) {
        const struct sentence *sentence = text->sentences[i];
        if (sentence == NULL) {
            fprintf(file, "0\n");
        } else {
            fprintf(file, "%"PRIlink, sentence->length);
            if (sentence->tokens[i] == 0) {
                perror("text_write(): NULL token in text");
                exit(1);
            }
            for (link_t j=0; j<sentence->length; j++) {
                fprintf(file, " %"PRItoken, sentence->tokens[j] - 1);
            }
            fprintf(file, "\n");
        }
    }
}

struct text* text_read(const char *filename) {
    FILE *file = (!strcmp(filename, "-"))? stdin: fopen(filename, "r");
    if (file == NULL) {
        perror("text_read(): failed to open text file");
        return NULL;
    }
    struct text *text = malloc(sizeof(struct text));
    if (text == NULL) {
        perror("text_read(): failed to allocate structure");
        if (file != stdin) fclose(file);
        return NULL;
    }
    if ((text->filename = malloc(strlen(filename)+1)) == NULL) {
        perror("text_read(): failed to allocate filename string");
        exit(EXIT_FAILURE);
    }
    strcpy(text->filename, filename);
    if (fscanf(file, "%zd %"SCNtoken"\n",
              &(text->n_sentences), &(text->vocabulary_size)) != 2)
    {
        fprintf(stderr,
                "text_read(): failed to read header in %s\n", filename);
        free(text);
        if (file != stdin) fclose(file);
        return NULL;
    }
    // type 0 is always reserved for NULL, so we need to increase vocabulary
    // size by one
    text->vocabulary_size++;
    text->sentences = malloc(text->n_sentences * sizeof(struct sentence*));
    if (text->sentences == NULL) {
        perror("text_read(): failed to allocate sentence array");
        exit(EXIT_FAILURE);
    }
    for (size_t i=0; i<text->n_sentences; i++)
        text->sentences[i] = sentence_read(file, text->vocabulary_size);
    if (file != stdin) fclose(file);
    return text;
}

static void align(
        int reverse,
        const struct text *source,
        const struct text *target,
        int model,
        int score_model,
        double null_prior,
        int n_samplers,
        int quiet,
        const int *n_iters,
        const char *links_filename,
        const char *stats_filename,
        const char *scores_filename,
        const char *priors_filename)
{
    double t0;
    random_state state;
    struct text_alignment *tas[n_samplers];

    random_system_state(&state);

    t0 = seconds();
    for (int i=0; i<n_samplers; i++) {
        tas[i] = text_alignment_create(
                (reverse? target: source), (reverse? source: target));
        tas[i]->null_prior = null_prior;
        if (priors_filename != NULL) {
            // TODO: since read-only, could use the pointer from tas[0]
            //       for everything, but this would require careful
            //       initialization/destruction
            if(text_alignment_load_priors(tas[i], priors_filename, reverse)) {
                fprintf(stderr, "Unable to load %s, exiting\n",
                        priors_filename);
                exit(1);
            }
        }
    }
    if (!quiet)
        fprintf(stderr, "Created alignment structures: %.3f s\n",
                seconds() - t0);

    t0 = seconds();
#pragma omp parallel for
    for (int i=0; i<n_samplers; i++) {
        random_state local_state;
#pragma omp critical
        {
            local_state = random_split_state(&state);
        }
        text_alignment_randomize(tas[i], &local_state);
    }
    if (!quiet)
        fprintf(stderr, "Randomized alignment: %.3f s\n", seconds() - t0);

    for (int m=1; m<=model; m++) {
        if (n_iters[m-1]) {
            if (!quiet)
                fprintf(stderr, "Aligning with model %d (%d iterations)\n",
                        m, n_iters[m-1]);
            t0 = seconds();

#pragma omp parallel for
            for (int i=0; i<n_samplers; i++) {
                random_state local_state;
#pragma omp critical
                {
                    local_state = random_split_state(&state);
                }
                tas[i]->model = m;

                text_alignment_make_counts(tas[i]);

                for (int j=0; j<n_iters[m-1]; j++) {
                    text_alignment_sample(tas[i], &local_state, NULL, NULL, 1);
                }
            }
            if (!quiet)
                fprintf(stderr, "Done: %.3f s\n", seconds() - t0);
        }
    }

    t0 = seconds();
    text_alignment_sample(tas[0], &state, NULL, tas, n_samplers);
    if (!quiet)
        fprintf(stderr, "Final argmax iteration: %.3f s\n", seconds() - t0);

    struct text_alignment *ta = tas[0];

    if (stats_filename != NULL) {
        if (!quiet)
            fprintf(stderr, "Writing alignment statistics to %s\n",
                    stats_filename);
        FILE *file = (!strcmp(stats_filename, "-"))? stdout
                     : fopen(stats_filename, "w");
        //text_alignment_write_vocab(ta, file);
        text_alignment_write_stats(ta, file);
        if (file != stdout) fclose(file);
    }

    if (links_filename != NULL) {
        if (!quiet)
            fprintf(stderr, "Writing alignments to %s for %Zu sentencess\n",
                    links_filename, ta->target->n_sentences);
        FILE *file = (!strcmp(links_filename, "-"))? stdout
                     : fopen(links_filename, "w");
        text_alignment_write_moses(ta, file, reverse);
        if (file != stdout) fclose(file);
    }

    if (scores_filename != NULL) {
        count *scores = malloc(sizeof(count)*ta->source->n_sentences);
        for (size_t i=0; i<ta->source->n_sentences; i++)
            scores[i] = (count)0.0;

        FILE *file = (!strcmp(scores_filename, "-"))? stdout
                     : fopen(scores_filename, "w");

        if (!quiet)
            fprintf(stderr,
                    "Computing scores using model %d for %Zu sentences\n",
                    score_model, ta->source->n_sentences);

        // Switch to whatever model is specified for scoring
        ta->model = score_model;
        text_alignment_sample(ta, &state, scores, NULL, 1);

        for (size_t i=0; i<ta->source->n_sentences; i++)
            fprintf(file, "%g\n", -scores[i]);

        if (file != stdout) fclose(file);
        free(scores);
    }


}

static void help(const char *filename) {
    fprintf(stderr,
"Usage: %s [-s source_input] [-t target_input] [-p priors_input] "
"[-f forward_links_output] "
"[-r reverse_links_output] [-S statistics_output] [-F forward_scores_output] "
"[-R reverse_scores_output] "
"[-1 n_IBM1_iters] [-2 n_HMM_iters] [-3 n_fertility_iters] "
"[-n n_samplers] [-N null_prior] [-q] [-M score_model] -m model_type\n",
        filename);
}

int main(int argc, char *argv[]) {
    double t0;
    int opt;
    char *source_filename = "-", *target_filename = "-",
         *priors_filename = NULL,
         *links_filename_fwd = NULL, *links_filename_rev = NULL,
         *stats_filename = NULL,
         *scores_filename_fwd = NULL, *scores_filename_rev = NULL;
    int n_iters[3];
    int n_samplers = 1, quiet = 0, model = -1, score_model = -1;
    double null_prior = 0.2;

    n_iters[0] = 1; n_iters[1] = 1; n_iters[2] = 1;

    omp_set_nested(1);

    while ((opt = getopt(argc, argv, "s:t:p:f:r:S:F:R:1:2:3:n:qm:M:N:h"))
            != -1)
    {
        switch(opt) {
            case 's': source_filename = optarg; break;
            case 't': target_filename = optarg; break;
            case 'p': priors_filename = optarg; break;
            case 'f': links_filename_fwd = optarg; break;
            case 'r': links_filename_rev = optarg; break;
            case 'F': scores_filename_fwd = optarg; break;
            case 'R': scores_filename_rev = optarg; break;
            case 'S': stats_filename = optarg; break;
            case '1': n_iters[0] = atoi(optarg); break;
            case '2': n_iters[1] = atoi(optarg); break;
            case '3': n_iters[2] = atoi(optarg); break;
            case 'n': n_samplers = atoi(optarg); break;
            case 'q': quiet = 1; break;
            case 'm': model = atoi(optarg);
                      if (model < 1 || model > 3) {
                          fprintf(stderr, "Model must be 1, 2 or 3!\n");
                          return 1;
                      }
                      break;
            case 'M': score_model = atoi(optarg);
                      if (score_model < 1 || score_model > 3) {
                          fprintf(stderr,
                                "(Scoring) model must be 1, 2 or 3!\n");
                          return 1;
                      }
                      break;
            case 'N': null_prior = atof(optarg); break;
            case 'h':
            default:
                help(argv[0]);
                return 1;
        }
    }

    if (model == -1) {
        help(argv[0]);
        return 1;
    }

    if (score_model == -1) score_model = model;

    t0 = seconds();
    struct text *source = text_read(source_filename);
    struct text *target = text_read(target_filename);
    if (source->n_sentences != target->n_sentences) {
        fprintf(stderr, "Source text has %zd sentences but target has %zd\n",
                source->n_sentences, target->n_sentences);
        return 1;
    }
    if (!quiet) {
        fprintf(stderr, "Read texts (%zd sentences): %.3f s\n",
                source->n_sentences, seconds() - t0);
        fprintf(stderr, "Vocabulary sizes are %"PRItoken" (source),"
                        " %"PRItoken" (target)\n",
                source->vocabulary_size, target->vocabulary_size);
    }

#pragma omp parallel for
    for (int reverse=0; reverse<=1; reverse++) {
        char *links_filename =
            (reverse? links_filename_rev: links_filename_fwd);
        char *scores_filename =
            (reverse? scores_filename_rev: scores_filename_fwd);
        if (links_filename != NULL ||
                scores_filename != NULL ||
                (!reverse && links_filename_fwd == NULL &&
                 links_filename_rev == NULL))
            align(reverse, source, target, model, score_model, null_prior,
                  n_samplers,
                  quiet, n_iters, links_filename, stats_filename,
                  scores_filename, priors_filename);
    }

    return 0;
}


/* random.c: functions for pseuro-random number generation.
 *
 * By default an xorshift* PRNG is used for uniform random number generation,
 * but this is configurable with the macros below the xss_* functions.
 */

#ifndef __RANDOM_C__
#define __RANDOM_C__

#include <math.h>
#include <float.h>
#include <time.h>

#include "hash.c"

typedef uint64_t xss_state;

// 64-bit xorshift*
static inline void xss_step(xss_state *state) {
    xss_state x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    x *= 2685821657736338717ULL;
    *state = x;
}

// this is used to seed new generators from an old state
static inline xss_state xss_split_state(xss_state *state) {
    xss_step(state);
    return hash_u64_u64(*state);
}

static inline double xss_uniform64(xss_state *state) {
    xss_step(state);
    return (double)(*state-1) / (double)0xffffffffffffffffULL;
}

static inline float xss_uniform32(xss_state *state) {
    xss_step(state);
    return (float)(*state-1) / (float)0xffffffffffffffffULL;
}

static inline uint32_t xss_uint32_biased(xss_state *state, uint32_t n) {
    xss_step(state);
    return (*state) % n;
}

static inline uint32_t xss_uint32_unbiased(xss_state *state, uint32_t n) {
    uint32_t max = 0x100000000ULL - (0x100000000ULL % (xss_state)n);
    while(1) {
        xss_step(state);
        uint32_t x = *state;
        if (x < max) return x % n;
    }
}

typedef xss_state               random_state;
#define random_uniform64        xss_uniform64
#define random_uniform32        xss_uniform32
#define random_uint32_biased    xss_uint32_biased
#define random_uint32_unbiased  xss_uint32_unbiased
#define random_split_state      xss_split_state


static int random_system_state(random_state *state) {
    FILE *file = fopen("/dev/urandom", "rb");
    if (file == NULL) {
        fprintf(stderr, "Warning: can not open /dev/urandom, using time()\n");
        *state = time(NULL);
        return 0;
    }
    fread(state, sizeof(*state), 1, file);
    fclose(file);
    return 0;
}

/* Sample from an unnormalized cumulative categorical distribution.
 * Since it is cumulative, the last element of p contains the normalization
 * factor. By subtracting each p[i] from p[i+1] and dividing by the original
 * p[length-1], we would obtain an ordinary categorical distribution.
 */
static inline size_t random_unnormalized_cumulative_categorical32(
        random_state *state, const float *p, size_t length)
{
    const float u = random_uniform32(state) * p[length-1];
    for (size_t i=0; i<length-1; i++)
        if (p[i] >= u) return i;
    return length-1;
}

static inline size_t random_unnormalized_log_categorical32(
        random_state *state, const float *log_p, float lambda, size_t length)
{
    float max_log_p = -FLT_MAX;
    for (size_t i=0; i<length; i++)
        max_log_p = fmaxf(log_p[i], max_log_p);
    float p[length];
    float p_sum = 0.0f;
    for (size_t i=0; i<length; i++) {
        const float log_p_i = (log_p[i] - max_log_p) * lambda;
        const float p_i = expf(log_p_i);
        p_sum += p_i;
        p[i] = p_sum;
    }
    return random_unnormalized_cumulative_categorical32(state, p, length);
}

/* Sample from gamma(alpha >= 1, 1)
 *
 * R. C. H. Cheng (1977)
 * The Generation of Gamma Variables with Non-Integral Shape Parameter
 * Journal of the Royal Statistical Society. Series C (Applied Statistics),
 * Vol. 26, No. 1 (1977), pp. 71--75
 */
static inline double random_gamma64(
        random_state *state, const double alpha)
{
    const double a = 1.0/sqrt(2.0*alpha - 1.0);
    const double b = alpha - log(4.0);
    const double c = alpha + 1.0/a;
    while(1) {
        const double u1 = random_uniform64(state);
        const double u2 = random_uniform64(state);
        const double v = a * log(u1 / (1.0-u1));
        const double x = alpha * exp(v);
        if (b + c*v - x >= log(u1*u1*u2)) return x;
    }
}

static inline float random_gamma32(
        random_state *state, const float alpha)
{
    const float a = 1.0/sqrtf(2.0*alpha - 1.0);
    const float b = alpha - logf(4.0);
    const float c = alpha + 1.0/a;
    while(1) {
        const float u1 = random_uniform32(state);
        const float u2 = random_uniform32(state);
        const float v = a * logf(u1 / (1.0-u1));
        const float x = alpha * expf(v);
        if (b + c*v - x >= logf(u1*u1*u2)) return x;
    }
}

/* Sample from gamma(alpha << 1, 1)
 *
 * http://arxiv.org/pdf/1302.1884.pdf
 * http://homepages.math.uic.edu/~rgmartin/Research/Codes/Gamma/rgamss.R
 */
static inline double random_log_gamma_small64(
        random_state *state, const double alpha)
{
    const double e = 2.7182818284590452354;
    const double lambda = (1.0 / alpha) - 1.0;
    const double w = alpha / (e * (1.0 - alpha));
    const double r = 1.0 / (1.0 + w);

    while(1) {
        const double u = random_uniform64(state);
        const double z = (u <= r)? -log(u/r)
                                 : log(random_uniform64(state))/lambda;
        const double h = exp(-z-exp(-z/alpha));
        const double eta = (z >= 0.0)? exp(-z): w*lambda*exp(lambda*z);
        if (h > eta*random_uniform64(state))
            return -z/alpha;
    }
}

static inline float random_log_gamma_small32(
        random_state *state, const float alpha)
{
    const float e = 2.7182818284590452354f;
    const float lambda = (1.0 / alpha) - 1.0;
    const float w = alpha / (e * (1.0 - alpha));
    const float r = 1.0 / (1.0 + w);

    while(1) {
        const float u = random_uniform32(state);
        const float z = (u <= r)? -logf(u/r)
                                 : logf(random_uniform32(state))/lambda;
        const float h = expf(-z-expf(-z/alpha));
        const float eta = (z >= 0.0)? expf(-z): w*lambda*expf(lambda*z);
        if (h > eta*random_uniform32(state))
            return -z/alpha;
    }
}

static void random_dirichlet64_unnormalized(
        random_state *state, size_t d, const double *alpha, double *x)
{
    for (size_t i=0; i<d; i++) {
        // Note: in the interval around 0.2 -- 0.9 there are better
        // options than either of these algorithms, but that's not a common
        // case in our applications.
        if (alpha[i] < 0.6) {
            x[i] = exp(random_log_gamma_small64(state, alpha[i]));
        } else {
            x[i] = random_gamma64(state, alpha[i]);
        }
    }
}

static void random_dirichlet32_unnormalized(
        random_state *state, size_t d, const float *alpha, float *x)
{
    for (size_t i=0; i<d; i++) {
        // Note: in the interval around 0.2 -- 0.9 there are better
        // options than either of these algorithms, but that's not a common
        // case in our applications.
        if (alpha[i] < 0.6f) {
            x[i] = expf(random_log_gamma_small32(state, alpha[i]));
        } else {
            x[i] = random_gamma32(state, alpha[i]);
        }
    }
}

#endif


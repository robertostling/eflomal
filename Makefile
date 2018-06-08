CFLAGS=-Ofast -march=native -Wall --std=gnu99 -Wno-unused-function -g -fopenmp
# This is more suitable for debugging:
#CFLAGS=-Og -Wall --std=gnu99 -Wno-unused-function -g -fopenmp
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS=-lm -lgomp -fopenmp
else
    LDFLAGS=-lm -lrt -lgomp -fopenmp
endif

all: eflomal

eflomal.o: eflomal.c natmap.c hash.c random.c simd_math_prims.h
	$(CC) $(CFLAGS) -c eflomal.c

eflomal: eflomal.o

install: eflomal
	install eflomal /usr/local/bin/eflomal

clean:
	rm -f eflomal eflomal.o


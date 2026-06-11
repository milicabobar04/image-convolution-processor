CC = gcc
CFLAGS_BASE = -Wall -Wextra -fopenmp -march=native
LDFLAGS = -lm
SOURCES = bmp_parser.c convolution.c
SOURCES_SIMD = bmp_parser.c convolution_simd.c
HEADERS = bmp_parser.h

.PHONY: all clean benchmark test

all: convolution_O0 convolution_O1 convolution_O2 convolution_O3 \
     convolution_simd_O0 convolution_simd_O1 convolution_simd_O2 convolution_simd_O3

convolution_O0: $(SOURCES) $(HEADERS)
	$(CC) -O0 $(CFLAGS_BASE) $(SOURCES) -o $@ $(LDFLAGS)

convolution_O1: $(SOURCES) $(HEADERS)
	$(CC) -O1 $(CFLAGS_BASE) $(SOURCES) -o $@ $(LDFLAGS)

convolution_O2: $(SOURCES) $(HEADERS)
	$(CC) -O2 $(CFLAGS_BASE) $(SOURCES) -o $@ $(LDFLAGS)

convolution_O3: $(SOURCES) $(HEADERS)
	$(CC) -O3 $(CFLAGS_BASE) $(SOURCES) -o $@ $(LDFLAGS)

convolution_simd_O0: $(SOURCES_SIMD) $(HEADERS)
	$(CC) -O0 $(CFLAGS_BASE) $(SOURCES_SIMD) -o $@ $(LDFLAGS)

convolution_simd_O1: $(SOURCES_SIMD) $(HEADERS)
	$(CC) -O1 $(CFLAGS_BASE) $(SOURCES_SIMD) -o $@ $(LDFLAGS)

convolution_simd_O2: $(SOURCES_SIMD) $(HEADERS)
	$(CC) -O2 $(CFLAGS_BASE) $(SOURCES_SIMD) -o $@ $(LDFLAGS)

convolution_simd_O3: $(SOURCES_SIMD) $(HEADERS)
	$(CC) -O3 $(CFLAGS_BASE) $(SOURCES_SIMD) -o $@ $(LDFLAGS)

benchmark: all
	chmod +x benchmark.sh
	./benchmark.sh

test: convolution_O3
	@echo "=== TEST TACNOSTI ALGORITMA ==="
	@python3 test_correctness.py

clean:
	rm -f convolution_O0 convolution_O1 convolution_O2 convolution_O3
	rm -f convolution_simd_O0 convolution_simd_O1 convolution_simd_O2 convolution_simd_O3
	rm -rf benchmark_results
	rm -rf benchmark_outputs
	rm -f *.bmp test_*.bmp
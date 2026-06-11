#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <math.h>
#include <immintrin.h>
#include "bmp_parser.h"

typedef struct {
    float *data;
    int size;
} Kernel;

static inline uint8_t clamp(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (uint8_t)v;
}

void apply_convolution_simd(BMPImage *src, BMPImage *dst, Kernel *kernel) {
    int w = src->width;
    int h = src->height;
    int k_size = kernel->size;
    int pad = k_size / 2; // dodajemo margine oko slike ( /2 jer kernelu treba centralni piksel)

    int p_w = w + 2 * pad; 
    int p_h = h + 2 * pad;
    int plane_size = p_w * p_h; // dimezije prosirene slike

    // pravimo bafere za svaku komponentu piksela
    // koristimo aligned_alloc jer trazimo od OSa memoriju cija adresa pocinje na 32
    // zato sto su AVX2 registri velicne 32 te ih procesor moze jednim potezom ucitati u registar
    float *buf_r = (float*)aligned_alloc(32, plane_size * sizeof(float));
    float *buf_g = (float*)aligned_alloc(32, plane_size * sizeof(float));
    float *buf_b = (float*)aligned_alloc(32, plane_size * sizeof(float));

    if (!buf_r || !buf_g || !buf_b) {
        fprintf(stderr, "Greska pri alokaciji memorije za SIMD bafere.\n");
        exit(1);
    }

    #pragma omp parallel for
    for (int y = 0; y < p_h; y++) {
        for (int x = 0; x < p_w; x++) {
            int src_y = y - pad; 
            int src_x = x - pad;
            if (src_y < 0) src_y = 0;
            if (src_y >= h) src_y = h - 1;
            if (src_x < 0) src_x = 0;
            if (src_x >= w) src_x = w - 1;

            int src_idx = src_y * src->row_padded + src_x * 3; // indeks u orginalnom nizu
            int buf_idx = y * p_w + x;                         // indeks u u novom nizu

            buf_b[buf_idx] = (float)src->data[src_idx + 0];
            buf_g[buf_idx] = (float)src->data[src_idx + 1];
            buf_r[buf_idx] = (float)src->data[src_idx + 2];
        }
    }

    float *planes[3] = {buf_b, buf_g, buf_r};

    
    #pragma omp parallel for
    for (int y = 0; y < h; y++) {
        for (int c = 0; c < 3; c++) {
            float *curr_plane = planes[c]; // biramo baffer
            

            for (int x = 0; x < w; x += 8) {
                int remaining = w - x;
                 __m256 sum = _mm256_setzero_ps();

                for (int ky = 0; ky < k_size; ky++) { 
                    float *row_ptr = &curr_plane[(y + ky) * p_w + x]; 

                    for (int kx = 0; kx < k_size; kx++) {
                        float kval = kernel->data[ky * k_size + kx];
                        __m256 v_kernel = _mm256_set1_ps(kval);
                        __m256 v_pixels = _mm256_loadu_ps(row_ptr + kx);

                        sum = _mm256_fmadd_ps(v_pixels, v_kernel, sum);
                    }
                }

                sum = _mm256_max_ps(sum, _mm256_setzero_ps());       // max(0, val)
                sum = _mm256_min_ps(sum, _mm256_set1_ps(255.0f));    // min(255, val)
                
                float res[8];
                _mm256_storeu_ps(res, sum);

                for (int i = 0; i < ((remaining < 8) ? remaining : 8); i++) {
                    int dst_idx = y * dst->row_padded + (x + i) * 3 + c;
                    dst->data[dst_idx] = (uint8_t)res[i];
                }
            }
        }
    }

    free(buf_r);
    free(buf_g);
    free(buf_b);
}

Kernel* parse_kernel(int argc, char **argv, int *arg_idx) {
    if (*arg_idx >= argc) {
        Kernel *k = (Kernel*)malloc(sizeof(Kernel));
        k->size = 3;
        k->data = (float*)malloc(9 * sizeof(float));
        float sharpen[] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
        memcpy(k->data, sharpen, 9 * sizeof(float));
        return k;
    }
    
    int size = atoi(argv[(*arg_idx)++]);
    if (size <= 0 || size % 2 == 0) {
        fprintf(stderr, "Kernel velicina mora biti neparan pozitivan broj\n");
        return NULL;
    }
    
    Kernel *k = (Kernel*)malloc(sizeof(Kernel));
    k->size = size;
    k->data = (float*)malloc(size * size * sizeof(float));
    
    for (int i = 0; i < size * size; i++) {
        if (*arg_idx >= argc) {
            fprintf(stderr, "Nedovoljno kernel vrijednosti\n");
            free(k->data);
            free(k);
            return NULL;
        }
        k->data[i] = atof(argv[(*arg_idx)++]);
    }
    
    return k;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Upotreba: %s <ulazni.bmp> <izlazni.bmp> [kernel_size k00 ... kNN]\n", argv[0]);
        return 1;
    }
    
    const char *input = argv[1];
    const char *output = argv[2];
    
    int arg_idx = 3;
    Kernel *kernel = parse_kernel(argc, argv, &arg_idx);
    if (!kernel) return 1;
    
    printf("SIMD Optimizovana Verzija (AVX2)\n");
    printf("Ucitavanje: %s\n", input);
    
    BMPImage *src = bmp_load(input);
    if (!src) {
        fprintf(stderr, "Greska pri ucitavanju\n");
        free(kernel->data);
        free(kernel);
        return 1;
    }
    
    printf("Velicina: %dx%d\n", src->width, src->height);
    
    BMPImage *dst = (BMPImage*)malloc(sizeof(BMPImage));
    dst->width = src->width;
    dst->height = src->height;
    dst->row_padded = src->row_padded;
    dst->data = (uint8_t*)malloc(dst->row_padded * dst->height);
    
    double start = omp_get_wtime();
    apply_convolution_simd(src, dst, kernel);
    double end = omp_get_wtime();
    
    printf("Vrijeme: %.4f sekundi\n", end - start);
    
    bmp_save(output, dst);
    
    bmp_free(src);
    bmp_free(dst);
    free(kernel->data);
    free(kernel);
    
    return 0;
}
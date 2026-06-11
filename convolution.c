#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <math.h>
#include "bmp_parser.h"

// Helper funkcije za clamping
static inline float clamp_float(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

typedef struct {
    float *data;
    int size;
} Kernel;

void apply_convolution(BMPImage *src, BMPImage *dst, Kernel *kernel) {
    int krad = kernel->size / 2;
    int w = src->width;
    int h = src->height;
    
    #pragma omp parallel for schedule(dynamic, 16) collapse(2)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
            
            for (int ky = -krad; ky <= krad; ky++) {
                for (int kx = -krad; kx <= krad; kx++) {
                    int px = x + kx;
                    int py = y + ky;
                    
                    if (px < 0) px = 0;
                    if (px >= w) px = w - 1;
                    if (py < 0) py = 0;
                    if (py >= h) py = h - 1;
                    
                    int src_idx = py * src->row_padded + px * 3;
                    int k_idx = (ky + krad) * kernel->size + (kx + krad);
                    float k_val = kernel->data[k_idx];
                    
                    sum_b += src->data[src_idx + 0] * k_val;
                    sum_g += src->data[src_idx + 1] * k_val;
                    sum_r += src->data[src_idx + 2] * k_val;
                }
            }
            
            int dst_idx = y * dst->row_padded + x * 3;
            dst->data[dst_idx + 0] = (uint8_t)clamp_float(sum_b, 0.0f, 255.0f);
            dst->data[dst_idx + 1] = (uint8_t)clamp_float(sum_g, 0.0f, 255.0f);
            dst->data[dst_idx + 2] = (uint8_t)clamp_float(sum_r, 0.0f, 255.0f);
        }
    }
}

Kernel* parse_kernel(int argc, char **argv, int *arg_idx) {
        if (*arg_idx >= argc) {
        Kernel *k = (Kernel*)malloc(sizeof(Kernel));
        k->size = 3;
        k->data = (float*)malloc(9 * sizeof(float));
        float blur[] = {0.1111,0.1111,0.1111,
                        0.1111,0.1111,0.1111,
                        0.1111,0.1111,0.1111};
        memcpy(k->data, blur, 9 * sizeof(float));
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

void print_usage(const char *prog) {
    printf("Upotreba: %s <ulazni.bmp> <izlazni.bmp> [kernel_size k00 k01 ... kNN]\n", prog);
    printf("Podrazumijevani kernel: 3x3 sharpen filter\n");
    printf("Primjer: %s in.bmp out.bmp 3 -1 -1 -1 -1 9 -1 -1 -1 -1\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *input = argv[1];
    const char *output = argv[2];
    
    int arg_idx = 3;
    Kernel *kernel = parse_kernel(argc, argv, &arg_idx);
    if (!kernel) return 1;
    
    printf("Ucitavanje slike: %s\n", input);
    BMPImage *src = bmp_load(input);
    if (!src) {
        fprintf(stderr, "Greska pri ucitavanju: %s\n", input);
        free(kernel->data);
        free(kernel);
        return 1;
    }
    
    printf("Velicina slike: %dx%d piksela\n", src->width, src->height);
    printf("Kernel: %dx%d\n", kernel->size, kernel->size);
    
    BMPImage *dst = (BMPImage*)malloc(sizeof(BMPImage));
    dst->width = src->width;
    dst->height = src->height;
    dst->row_padded = src->row_padded;
    dst->data = (uint8_t*)malloc(dst->row_padded * dst->height);
    
    double start = omp_get_wtime();
    apply_convolution(src, dst, kernel);
    double end = omp_get_wtime();
    
    printf("Vrijeme obrade: %.4f sekundi\n", end - start);
    printf("Cuvanje rezultata: %s\n", output);
    
    if (!bmp_save(output, dst)) {
        fprintf(stderr, "Greska pri cuvanju: %s\n", output);
    }
    
    bmp_free(src);
    bmp_free(dst);
    free(kernel->data);
    free(kernel);
    
    return 0;
}
#include "bmp_parser.h"
#include <stdlib.h>
#include <string.h>

BMPImage* bmp_load(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return NULL;
    }

    BMPHeader header;
    BMPInfoHeader info;

    if (fread(&header, sizeof(BMPHeader), 1, f) != 1) {
        fprintf(stderr, "Cannot read BMP header\n");
        fclose(f);
        return NULL;
    }

    if (header.type != 0x4D42) {
        fprintf(stderr, "Not a BMP file (type: 0x%X)\n", header.type);
        fclose(f);
        return NULL;
    }

    if (fread(&info, sizeof(BMPInfoHeader), 1, f) != 1) {
        fprintf(stderr, "Cannot read BMP info header\n");
        fclose(f);
        return NULL;
    }

    if (info.bits != 24 || info.compression != 0) {
        fprintf(stderr, "Unsupported BMP format (bits: %d, compression: %d)\n", 
                info.bits, info.compression);
        fclose(f);
        return NULL;
    }

    BMPImage *img = (BMPImage*)malloc(sizeof(BMPImage));
    if (!img) {
        fprintf(stderr, "Cannot allocate BMPImage structure\n");
        fclose(f);
        return NULL;
    }

    img->width = info.width;
    img->height = abs(info.height);
    img->row_padded = ((img->width * 3 + 3) / 4) * 4;

    size_t data_size = img->row_padded * img->height;
    fprintf(stderr, "DEBUG: Allocating %zu bytes for image data\n", data_size);
    
    img->data = (uint8_t*)malloc(data_size);
    if (!img->data) {
        fprintf(stderr, "Cannot allocate image data (%zu bytes)\n", data_size);
        free(img);
        fclose(f);
        return NULL;
    }

    fseek(f, header.offset, SEEK_SET);
    
    if (info.height > 0) {
        // Bottom-up BMP
        for (int y = img->height - 1; y >= 0; y--) {
            size_t read = fread(img->data + y * img->row_padded, 1, img->row_padded, f);
            if (read != (size_t)img->row_padded) {
                fprintf(stderr, "Error reading row %d\n", y);
                free(img->data);
                free(img);
                fclose(f);
                return NULL;
            }
        }
    } else {
        // Top-down BMP
        size_t read = fread(img->data, 1, data_size, f);
        if (read != data_size) {
            fprintf(stderr, "Error reading image data\n");
            free(img->data);
            free(img);
            fclose(f);
            return NULL;
        }
    }

    fclose(f);
    fprintf(stderr, "DEBUG: Successfully loaded %dx%d BMP\n", img->width, img->height);
    return img;
}

int bmp_save(const char *filename, BMPImage *img) {
    FILE *f = fopen(filename, "wb");
    if (!f) return 0;

    int row_padded = ((img->width * 3 + 3) / 4) * 4;
    int filesize = 54 + row_padded * img->height;

    BMPHeader header = {
        .type = 0x4D42,
        .size = filesize,
        .reserved1 = 0,
        .reserved2 = 0,
        .offset = 54
    };

    BMPInfoHeader info = {
        .size = 40,
        .width = img->width,
        .height = img->height,
        .planes = 1,
        .bits = 24,
        .compression = 0,
        .imagesize = row_padded * img->height,
        .xresolution = 2835,
        .yresolution = 2835,
        .ncolours = 0,
        .importantcolours = 0
    };

    fwrite(&header, sizeof(BMPHeader), 1, f);
    fwrite(&info, sizeof(BMPInfoHeader), 1, f);

    for (int y = img->height - 1; y >= 0; y--) {
        fwrite(img->data + y * img->row_padded, 1, row_padded, f);
    }

    fclose(f);
    return 1;
}

void bmp_free(BMPImage *img) {
    if (img) {
        free(img->data);
        free(img);
    }
}
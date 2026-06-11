#pragma once

#include <stdint.h>
#include <stdio.h>

#pragma pack(push, 1) // poravnavamo bajt po bajt bez paddinga jer bmp format ima tacno definisan raspored

/*
* BMP header (14 bajtova)
*/
typedef struct{
    uint16_t type;      // tip fajla: mora biti 'BM'
    uint32_t size;      // velicina bmp fajla u bajtovima
    uint16_t reserved1; // rezervisano polje
    uint16_t reserved2; // rezervisano polje
    uint32_t offset;    // offset do pocetka podataka o pikselima
}BMPHeader;

/*
 * BMPInfoHeader (40 bajtova)
 */
typedef struct{
    uint32_t size;            // Veličina ovog zaglavlja 
    int32_t width;            // Širina slike u pikselima
    int32_t height;           // Visina slike u pikselima
    uint16_t planes;          // Broj ravni 
    uint16_t bits;            // Broj bitova po pikselu 
    uint32_t compression;     // Tip kompresije 0 ako nije 
    uint32_t imagesize;       // Veličina podataka slike
    int32_t xresolution;      // Horizontalna rezolucija (pikseli po metru)
    int32_t yresolution;      // Vertikalna rezolucija (pikseli po metru)
    uint32_t ncolours;        // Broj boja u paleti 
    uint32_t importantcolours;// Broj važnih boja 
}BMPInfoHeader;

#pragma pack(pop) // vracamo staro poravanje strukura

typedef struct{
    uint8_t *data; // RGB podaci
    int32_t width; // sirina slike u pikselima
    int32_t height; //visina slike u pikselima
    int32_t row_padded; //duzina jednog reda sa poravnjanem
}BMPImage;

BMPImage* bmp_load(const char *filename);
int bmp_save(const char* filename, BMPImage *image);
void bmp_free(BMPImage *img);




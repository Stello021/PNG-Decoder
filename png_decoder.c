#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "zlib.h"
#define SDL_MAIN_HANDLED
#include "SDL.h"


#define BYTES_PER_PIXEL 4  

typedef struct chunk
{
    unsigned int length;
    char* type;
    char* data;
}Chunk;

typedef struct 
{
    unsigned int width, height;
    unsigned char bitDepth, color, compression, filter, interlace;
}IHDR;

unsigned int ToLittleEndian(unsigned int value)
{
    return ((value & 0xFF) << 24) | 
    (((value >> 8) & 0xFF) << 16) | 
    (((value >> 16) & 0xFF) << 8) | 
    ((value >> 24) & 0xFF);
}


void ReadChunk(FILE* file, Chunk* chunk)
{
    if(feof(file))
    {
        chunk -> type = NULL;
        chunk -> data = NULL;
        return;
    }
    unsigned int chunk_length;

    //                   4 byte 
    fread(&chunk_length, sizeof(unsigned int), 1, file);
    chunk_length = ToLittleEndian(chunk_length);
    chunk -> length = chunk_length;
    printf("Length: %u\n", chunk -> length);

    chunk -> type = (char*)malloc(5); //allocate 4 byte for chunk type + null terminator
    fread(chunk -> type, sizeof(char), 4, file);
    (chunk -> type)[4] = '\0'; // Null-terminate the chunk type


    chunk -> data = (char*)malloc(chunk -> length);
    fread(chunk -> data, sizeof(char), chunk -> length, file);


    unsigned int crc;
    fread(&crc, sizeof(unsigned int), 1, file);
    crc = ToLittleEndian(crc);
    printf("CRC: %x\n", crc);

}

void ParseIHDR(const char* data, IHDR* ihdr)
{
    ihdr -> width = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]; //4 bytes
    ihdr -> height = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]; //4 bytes
    ihdr -> bitDepth = data[8];
    ihdr -> color = data[9];
    ihdr -> compression = data[10];
    if(ihdr -> compression != 0)
    {
        printf("Invalid compression method\n");
    }
    
    ihdr -> filter = data[11];
    if(ihdr -> filter != 0)
    {
        printf("Invalid filter method\n");
    }
    ihdr -> interlace = data[12];

}

void ConcatenateIDATData(const Chunk* chunks, size_t chunks_number, unsigned char** idat_data, unsigned long* idat_length)
{
    // Calculate the total length of IDAT data
    size_t total_idat_length = 0;
    for (size_t i = 0; i < chunks_number; i++)
    {
        if(strcmp(chunks[i].type, "IDAT") == 0)
        {
            total_idat_length += chunks[i].length;
        }
    }
    // Allocate memory for concatenated IDAT data
    *idat_data = (unsigned char*)malloc(total_idat_length + 1); // +1 for null terminator
    // Concatenate IDAT data
    *idat_length = 0;
    for (size_t i = 0; i < chunks_number; i++)
    {
        if(strcmp(chunks[i].type, "IDAT") == 0)
        {
            size_t chunk_data_length = chunks[i].length;
            for (size_t j = 0; j < chunk_data_length; j++)
            {
                (*idat_data)[*idat_length] = chunks[i].data[j];
                (*idat_length)++;
            }
        }    
    }

    // Null-terminate the concatenated data
    (*idat_data)[*idat_length] = '\0';
}

unsigned char PaethPredictor(const unsigned char a, unsigned char b, unsigned char c)
{
    unsigned char p = a + b - c;
    unsigned char result;
    if(abs(p - a) <= abs(p - b) && abs(p - a) <= abs(p - c))
    {
        result = a;
    }
    else if(abs(p - b) <= abs(p - c))
    {
        result = b;
    }
    else
    {
        result = c;
    }
    return result;
}
unsigned char Recon_a(unsigned char* recon, unsigned char stride, unsigned int row, unsigned char c)
{
    if(c >= BYTES_PER_PIXEL)
    {
        return recon[row * stride + c - BYTES_PER_PIXEL];
    }
    return 0;
}

unsigned char Recon_b(unsigned char* recon, unsigned char stride, unsigned int row, unsigned char c)
{
    if(row > 0)
    {
        return recon[(row - 1) * stride + c];
    }
    return 0;
}
unsigned char Recon_c(unsigned char* recon, unsigned char stride, unsigned int row, unsigned char c)
{
    if(row > 0 && c >= BYTES_PER_PIXEL)
    {
        return recon[(row - 1) * stride + c - BYTES_PER_PIXEL];
    }
    return 0;
}
int main(int argc, char const *argv[])
{
    FILE* png_file;
    fopen_s(&png_file,"basn6a08.png", "rb");
    if(!png_file)
    {
        printf("Error opening file\n");
        return 1;
    }
    unsigned char png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    unsigned char file_signature[8];
    size_t bytesRead = fread(file_signature, 1, sizeof(file_signature), png_file);
    if(bytesRead != sizeof(file_signature) || memcmp(file_signature, png_signature, sizeof(file_signature)) != 0)
    {
        printf("Invalid PNG Signature\n");
        return 1;
    }
    Chunk* chunks = NULL;
    size_t chunks_number = 0;

    while(1)
    {
        Chunk new_chunk;

        ReadChunk(png_file, &new_chunk);
        if(new_chunk.type == NULL)
        {
            break;
        }
        //Add chunk
        chunks = (Chunk*)realloc(chunks, (chunks_number + 1) * sizeof(Chunk));
        chunks[chunks_number] = new_chunk;

        printf("Chunk Type: %s\n", chunks[chunks_number].type);

        if(strcmp(chunks[chunks_number].type, "IEND") == 0)
        {
            break;
        }

        chunks_number++;
        printf("----------\n");

    }
    //Parse IHDR data
    IHDR ihdrChunk;
    if(chunks_number > 0)
    {
        ParseIHDR(chunks[0].data, &ihdrChunk);
        printf("----------\n");
        printf("IHDR Data\n");
        printf("Width: %u\n", ihdrChunk.width);
        printf("Height: %u\n", ihdrChunk.height);
        printf("Bit Depth: %u\n", ihdrChunk.bitDepth);
        printf("Color Type: %u\n", ihdrChunk.color);
        printf("Compression Method: %u\n", ihdrChunk.compression);
        printf("Filter Method: %u\n", ihdrChunk.filter);
        printf("Interlace Method: %u\n", ihdrChunk.interlace);
    }
    printf("----------\n");
    // Concatenate IDAT data
    unsigned char* idat_data = NULL;
    unsigned long idat_length = 0;
    ConcatenateIDATData(chunks, chunks_number, &idat_data, &idat_length);
    printf("Concatenated IDAT Data Length: %lu\n", idat_length);
    printf("Concatenated IDAT Data:");
    for (size_t i = 0; i < 10; i++)
    {
       if(idat_data[i] == '\0')
        {
            printf("00 ");
        }
        else
        {
            printf("%02x ", (unsigned char)idat_data[i]);
        }
    }
    printf("[+%lu]\n",idat_length - 10);
    
    printf("----------\n");
    unsigned long uncompress_size = ihdrChunk.height * (1 + ihdrChunk.width * BYTES_PER_PIXEL);
    unsigned char* uncompress_data = malloc(uncompress_size);
    int result = uncompress(uncompress_data, &uncompress_size, idat_data, idat_length);
    if (result != Z_OK)
    {
        printf("unable to uncompress: error %d\n", result);
        free(idat_data);
        free(uncompress_data);
        return -1;
    }
    printf("Decompressed IDAT Data Length: %lu\n", uncompress_size);
    printf("Decompressed IDAT Data:");
    for (size_t i = 0; i < 10; i++)
    {
        if(uncompress_data[i] == '\0')
        {
            printf("00 ");
        }
        else
        {
            printf("%02x ", (unsigned char)uncompress_data[i]);
        }
    }
    printf("[+%lu]\n",uncompress_size - 10);
    printf("----------\n");

    const unsigned char stride = ihdrChunk.width * BYTES_PER_PIXEL;
    const size_t recon_size = stride * ihdrChunk.height;
    unsigned char* recon = (unsigned char*)malloc(recon_size);
    size_t recon_length = 0;
    size_t recon_index = 0;
    for (unsigned int y = 0; y < ihdrChunk.height; y++)
    {
        unsigned char filter_type = uncompress_data[recon_length];
        recon_length++;
        for (unsigned int x = 0; x < stride; x++)
        {
            unsigned char filter_x = uncompress_data[recon_length];
            recon_length++;
            unsigned char recon_x = 0;
            if(filter_type == 0)
            {
                recon_x = filter_x;
            }
            else if(filter_type == 1)
            {
                recon_x = filter_x + Recon_a(recon, stride, y, x);
            }
            else if(filter_type == 2)
            {
                recon_x = filter_x + Recon_b(recon, stride, y, x);
            }
            else if(filter_type == 3)
            {
                recon_x = (filter_x + (Recon_a(recon, stride, y, x) + Recon_b(recon, stride, y, x))) / 2;
            }
            else if(filter_type == 4)
            {
                recon_x = filter_x + PaethPredictor(Recon_a(recon, stride, y, x), Recon_b(recon, stride, y, x), Recon_c(recon, stride, y, x));
            }
            else
            {
                printf("Unknown filter type\n");
            }
            recon[recon_index] = recon_x;
            recon_index++;
        }
    }
    printf("Defiltered IDAT Data Length: %zu\n", recon_size);
    printf("Defiltered IDAT Data:");
    for (size_t i = 0; i < 10; i++)
    {
        if(recon[i] == '\0')
        {
            printf("00 ");
        }
        else
        {
            printf("%02x ", (unsigned char)recon[i]);
        }
    }
    printf("[+%zu]\n",recon_size - 10);
    printf("----------\n");

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        SDL_Log("Error initializing SDL2: %s", SDL_GetError());
        return -1;
    }
    SDL_Window *window = SDL_CreateWindow("Decoder PNG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512,  512, 0);
    if (!window)
    {
        SDL_Log("Error creating SDL2 Window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, ihdrChunk.width, ihdrChunk.height);
    if(texture == NULL)
    {
        printf("Texture creation failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    if(SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0)
    {
        printf("Blend mode failed: %s\n", SDL_GetError());
        return -1;
    }
    if(SDL_UpdateTexture(texture, NULL, recon, ihdrChunk.width * BYTES_PER_PIXEL) != 0)
    {
        printf("Update Texture failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_Event event;
    int window_opened = 1;
    while (window_opened)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                window_opened = 0;
            }
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(texture);

    SDL_Quit();

    //free 
    for (size_t i = 0; i < chunks_number; i++) 
    {
        free(chunks[i].type);
        free(chunks[i].data);
    }
    free(chunks);
    free(idat_data);
    free(uncompress_data);
    free(recon);


    return 0;
}


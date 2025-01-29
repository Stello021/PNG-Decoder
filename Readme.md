# PNG Decoder

## Overview

PNG decoder implemented in C, utilizing `zlib` for decompression and `SDL2` for visualization. The program reads a PNG file, processes its chunks, decompresses the image data, applies filtering, and displays the image.

## Usage

The program will read and decode `basn6a08.png` by default. You can modify the filename in the `main` function:

```c
FILE* png_file;
fopen_s(&png_file, "basn6a08.png", "rb");
if (!png_file) {
    printf("Error opening file\n");
    return 1;
}
```

## Key Points

### PNG chunks structure

Defines the basic PNG chunk format, storing length, type, and data.

```c
typedef struct chunk {
    unsigned int length;
    char* type;
    char* data;
} Chunk;
```

### IHDR

Represents and parsing the PNG header chunk containing metadata about the image.

```c
typedef struct ihdr {
    unsigned int width, height;
    unsigned char bitDepth, color, compression, filter, interlace;
} IHDR;


void ParseIHDR(const char* data, IHDR* ihdr) {
    ihdr->width = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    ihdr->height = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
}
```

### Reading chunks

Reads a chunk from the PNG file and extracts type and length information.

```c
void ReadChunk(FILE* file, Chunk* chunk) {
    if (feof(file)) {
        chunk->type = NULL;
        chunk->data = NULL;
        return;
    }
    fread(&chunk->length, sizeof(unsigned int), 1, file);
    chunk->length = ToLittleEndian(chunk->length);
    chunk->type = (char*)malloc(5);
    fread(chunk->type, sizeof(char), 4, file);
    chunk->type[4] = '\0';
}
```

### Concatenating IDAT Data

Combines multiple chunks into a single data block for decompression.

```c
void ConcatenateIDATData(const Chunk* chunks, size_t chunks_number, unsigned char** idat_data, unsigned long* idat_length) {
    size_t total_idat_length = 0;
    for (size_t i = 0; i < chunks_number; i++) {
        if (strcmp(chunks[i].type, "IDAT") == 0) {
            total_idat_length += chunks[i].length;
        }
    }
    *idat_data = (unsigned char*)malloc(total_idat_length + 1);
}
```

## Notes

- The image is displayed in a SDL2 window and closes when the user exits

- All allocated memory is freed before terminating



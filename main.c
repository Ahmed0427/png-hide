#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PNG_SIG_SIZE 8

const int PNG_SIG[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void) {
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (unsigned long) n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/*
  Update a running CRC with the bytes buf[0..len-1]--the CRC
  should be initialized to all 1's, and the transmitted value
  is the 1's complement of the final running CRC (see the
  crc() routine below)). 
*/

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len) {
    unsigned long c = crc;
    int n;

    if (!crc_table_computed) make_crc_table();

    for (n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long crc(unsigned char *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

void read_bytes(void* buffer, size_t buffer_size, FILE* file) {
    size_t read_size = fread(buffer, 1, buffer_size, file);
    if (read_size != buffer_size) {
        fprintf(stderr, "ERROR: could not read %zu bytes from file\n",
                buffer_size);
        exit(1);
    } 
}

void write_bytes(void* buffer, size_t buffer_size, FILE* file) {
    size_t read_size = fwrite(buffer, 1, buffer_size, file);
    if (read_size != buffer_size) {
        fprintf(stderr, "ERROR: could not write %zu bytes to file: %s\n",
                buffer_size, strerror(errno));
        exit(1);
    } 
}

bool is_valid_signature(uint8_t* signature) {
    for (int i = 0; i < PNG_SIG_SIZE; i++) {
        if (signature[i] != PNG_SIG[i]) {
            fprintf(stderr, "ERROR: this file is not a PNG image\n");
            return false;
        }
    }
    return true;
}

void reverse_bytes(void* data, size_t size) {
    uint8_t* bytes = data;
    for (int i = 0; i < size / 2; i++) {
        uint8_t t = bytes[i];
        bytes[i] = bytes[size - i - 1];
        bytes[size - i - 1] = t;
    }
}

void xor_cipher(uint8_t *text, int32_t size) {
    uint8_t key = 0xAF;
    for (int i = 0; i < size; i++) {
        text[i] ^= key;
    }
}

void inject_data(FILE* data_file, FILE* out_file, FILE* img_file) {
    if (fseek(data_file, 0L, SEEK_END) == -1) {
        fprintf(stderr, "ERROR: could not seek to end file data file\n");

        fclose(img_file);
        fclose(data_file);
        fclose(out_file);
        exit(1);
    }

    int32_t data_size = ftell(data_file);
    rewind(data_file);

    reverse_bytes(&data_size, sizeof(data_size));
    write_bytes(&data_size, sizeof(data_size), out_file);
    reverse_bytes(&data_size, sizeof(data_size));

    char* chunk_type = "inFo";
    write_bytes(chunk_type, sizeof(chunk_type), out_file);

    uint8_t data[data_size];
    read_bytes(data, data_size, data_file);

    xor_cipher(data, data_size);
    write_bytes(data, data_size, out_file);

    uint32_t chunk_crc = crc(data, data_size);
    write_bytes(&chunk_crc, sizeof(chunk_crc), out_file);
}

void injector(char** argv) {
    FILE* out_file = fopen("out.png", "wb");
    FILE* img_file = fopen(argv[1], "rb");
    FILE* data_file = fopen(argv[2], "rb");


    if (!img_file || !data_file || !out_file) {
        fprintf(stderr, "ERROR: could not open files\n");

        if (img_file) fclose(img_file);
        if (data_file) fclose(data_file);
        if (out_file) fclose(out_file);
        exit(1);
    }
    
    uint8_t signature[PNG_SIG_SIZE] = {0};
    read_bytes(signature, PNG_SIG_SIZE, img_file);
    write_bytes(signature, PNG_SIG_SIZE, out_file);
    if (!is_valid_signature(signature)) {
        fclose(img_file);
        fclose(data_file);
        fclose(out_file);
        exit(1);
    }

    while (1) {
        uint32_t chunk_len;
        read_bytes(&chunk_len, sizeof(chunk_len), img_file);
        write_bytes(&chunk_len, sizeof(chunk_len), out_file);
        reverse_bytes(&chunk_len, sizeof(chunk_len));

        uint8_t chunk_type[4];
        read_bytes(chunk_type, sizeof(chunk_type), img_file);
        write_bytes(chunk_type, sizeof(chunk_type), out_file);

        uint8_t chunk_data[chunk_len];
        read_bytes(chunk_data, chunk_len, img_file);
        write_bytes(chunk_data, chunk_len, out_file);

        uint32_t chunk_crc;
        read_bytes(&chunk_crc, sizeof(chunk_crc), img_file);
        write_bytes(&chunk_crc, sizeof(chunk_crc), out_file);

        if (strncmp((char*)chunk_type, "IEND", 4) == 0) {
            break;
        }
    }

    inject_data(data_file, out_file, img_file);

    fclose(img_file);
    fclose(data_file);
    fclose(out_file);
}

void extractor(char** argv) {
    FILE* img_file = fopen(argv[1], "rb");

    if (!img_file) {
        fprintf(stderr, "ERROR: could not open img file %s\n", argv[1]);
        exit(1);
    }
    
    uint8_t signature[PNG_SIG_SIZE] = {0};
    read_bytes(signature, PNG_SIG_SIZE, img_file);
    if (!is_valid_signature(signature)) {
        fclose(img_file);
        exit(1);
    }

    while (1) {
        uint32_t chunk_len;
        read_bytes(&chunk_len, sizeof(chunk_len), img_file);
        reverse_bytes(&chunk_len, sizeof(chunk_len));

        uint8_t chunk_type[4];
        read_bytes(chunk_type, sizeof(chunk_type), img_file);

        uint8_t chunk_data[chunk_len];
        read_bytes(chunk_data, chunk_len, img_file);

        uint32_t chunk_crc;
        read_bytes(&chunk_crc, sizeof(chunk_crc), img_file);

        if (strncmp((char*)chunk_type, "IEND", 4) == 0) {
            break;
        }
    }


    uint32_t chunk_len;
    size_t read_size = fread(&chunk_len, 1, sizeof(chunk_len), img_file);
    if (read_size != sizeof(chunk_len)) {
        fprintf(stderr, "No hidden data in this image\n");
        exit(1);
    } 
    reverse_bytes(&chunk_len, sizeof(chunk_len));

    uint8_t chunk_type[4];
    read_bytes(chunk_type, sizeof(chunk_type), img_file);

    uint8_t chunk_data[chunk_len];
    read_bytes(chunk_data, chunk_len, img_file);

    uint32_t chunk_crc;
    read_bytes(&chunk_crc, sizeof(chunk_crc), img_file);

    xor_cipher(chunk_data, chunk_len);
    printf("%.*s\n", chunk_len, (char*)chunk_data);

    fclose(img_file);
}

int main(int argc, char** argv) {
    if (argc == 3) {
        injector(argv);
    }
    else if (argc == 2) {
        extractor(argv);
    }
    else {
        fprintf(stderr, "Usage: %s <file.png> <data_file>\n", argv[0]);
        exit(1);
    }

    return 0;
}

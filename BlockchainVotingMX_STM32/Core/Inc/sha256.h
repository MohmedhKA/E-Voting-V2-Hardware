/*******************************************************************************
 * @file    sha256.h
 * @brief   SHA256 Cryptographic Hash Implementation
 * @author  Brad Conte (brad AT bradconte.com)
 * @note    Adapted for STM32L4
 ******************************************************************************/

#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SHA256_BLOCK_SIZE 32  // SHA256 outputs 32 byte digest

typedef unsigned char BYTE;
typedef unsigned int  WORD;

typedef struct {
    BYTE data[64];
    WORD datalen;
    unsigned long long bitlen;
    WORD state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[]);

/* Convenience function */
void sha256_hash(const BYTE *data, size_t len, BYTE hash[SHA256_BLOCK_SIZE]);


#endif /* SHA256_H */

// Copyright 2026 Zack Scholl, GPLv3.0
#include "seek_hash.h"
#include <string.h>
static uint32_t rotr(uint32_t v, unsigned n) { return (v>>n) | (v<<(32-n)); }
static void transform(seek_sha256_t *s) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[16], a=s->h[0], b=s->h[1], c=s->h[2], d=s->h[3];
    uint32_t e=s->h[4], f=s->h[5], g=s->h[6], h=s->h[7];
    for (unsigned i=0;i<64;++i) {
        if (i<16) {
            const uint8_t *p=s->block+i*4;
            w[i]=(uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
        } else {
            uint32_t x=w[(i-15)&15], y=w[(i-2)&15];
            w[i&15] += (rotr(x,7)^rotr(x,18)^(x>>3)) + w[(i-7)&15] + (rotr(y,17)^rotr(y,19)^(y>>10));
        }
        uint32_t t1=h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^((~e)&g))+k[i]+w[i&15];
        uint32_t t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
void seek_sha256_init(seek_sha256_t *s) {
    const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(s->h,initial,sizeof initial);s->bytes=0;
}
void seek_sha256_update(seek_sha256_t *s,const void *data,size_t bytes) {
    const uint8_t *p=data;
    while(bytes) {
        unsigned used=s->bytes&63, n=64-used;
        if(n>bytes)n=bytes;
        memcpy(s->block+used,p,n);s->bytes+=n;p+=n;bytes-=n;
        if(!(s->bytes&63))transform(s);
    }
}
void seek_sha256_final(seek_sha256_t *s,uint8_t digest[32]) {
    uint64_t bits=s->bytes*8;
    const uint8_t one=0x80, zero=0;
    seek_sha256_update(s,&one,1);
    while((s->bytes&63)!=56)seek_sha256_update(s,&zero,1);
    uint8_t length[8];for(unsigned i=0;i<8;++i)length[7-i]=bits>>(i*8);
    seek_sha256_update(s,length,8);
    for(unsigned i=0;i<32;++i)digest[i]=s->h[i/4]>>(24-(i%4)*8);
}
uint32_t seek_crc32(const void *data,size_t bytes) {
    const uint8_t *p=data;uint32_t crc=~0u;
    while(bytes--) {crc^=*p++;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));}
    return ~crc;
}

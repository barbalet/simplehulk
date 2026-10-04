/* Single-game freestanding runtime. Only the engine's %s/%d/%c formats are used. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static union { uint64_t align; unsigned char bytes[32768]; } arena;
static int allocated;
void *memset(void *p,int value,size_t n) { unsigned char *d=p; while(n--) *d++=(unsigned char)value;return p; }
void *memcpy(void *p,const void *q,size_t n) { unsigned char *d=p;const unsigned char *s=q;while(n--) *d++=*s++;return p; }
void *memmove(void *p,const void *q,size_t n) { unsigned char *d=p;const unsigned char *s=q;if(d<s) return memcpy(p,q,n);while(n) {n--;d[n]=s[n];}return p; }
char *strchr(const char *s,int c) { do {if(*s==(char)c) return (char *)s;}while(*s++);return NULL; }
int abs(int n) {return n<0?-n:n;}
void *calloc(size_t n,size_t size) {if(allocated || (size && n>sizeof arena.bytes/size)) return NULL;allocated=1;return memset(arena.bytes,0,n*size);}
void free(void *p) {if(p==arena.bytes) allocated=0;}
static void put(char *out,size_t cap,size_t *n,char c) {if(*n+1<cap) out[*n]=c;(*n)++;}
int vsnprintf(char *out,size_t cap,const char *fmt,va_list args) {
    size_t n=0;
    while(*fmt) {
        if(*fmt!='%') {put(out,cap,&n,*fmt++);continue;}
        fmt++;
        if(*fmt=='s') {const char *s=va_arg(args,const char *);if(!s)s="(null)";while(*s)put(out,cap,&n,*s++);}
        else if(*fmt=='c') put(out,cap,&n,(char)va_arg(args,int));
        else if(*fmt=='d') {int v=va_arg(args,int);unsigned u;char digits[12];int count=0;if(v<0)put(out,cap,&n,'-');u=v<0?0u-(unsigned)v:(unsigned)v;do {digits[count++]=(char)('0'+u%10);u/=10;}while(u);while(count)put(out,cap,&n,digits[--count]);}
        else put(out,cap,&n,*fmt);
        if(*fmt)fmt++;
    }
    if(cap)out[n<cap?n:cap-1]=0;
    return (int)n;
}
int snprintf(char *out,size_t cap,const char *fmt,...) {int n;va_list args;va_start(args,fmt);n=vsnprintf(out,cap,fmt,args);va_end(args);return n;}

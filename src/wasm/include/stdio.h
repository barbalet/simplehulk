#ifndef SH_WASM_STDIO
#define SH_WASM_STDIO
#include <stddef.h>
#include <stdarg.h>
int vsnprintf(char *, size_t, const char *, va_list);
int snprintf(char *, size_t, const char *, ...);
#endif

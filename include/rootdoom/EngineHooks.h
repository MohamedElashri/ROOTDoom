// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Included before engine C headers so system declarations retain their ABI;
// only engine call sites are redirected. C is compiled with unwind support.
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
void RootDoom_Exit(int status);
void RootDoom_ReportError(const char* format, va_list args);
char* RootDoom_StateDirectory(void);
void* RootDoom_Malloc(size_t size);
void* RootDoom_Calloc(size_t count, size_t size);
void* RootDoom_Realloc(void* pointer, size_t size);
void RootDoom_Free(void* pointer);
char* RootDoom_Strdup(const char* text);
FILE* RootDoom_Fopen(const char* path, const char* mode);
int RootDoom_Fclose(FILE* file);
#ifdef __cplusplus
}
#endif

#ifdef ROOTDOOM_ENGINE
#define exit RootDoom_Exit
#define malloc RootDoom_Malloc
#define calloc RootDoom_Calloc
#define realloc RootDoom_Realloc
#define free RootDoom_Free
#define strdup RootDoom_Strdup
#define fopen RootDoom_Fopen
#define fclose RootDoom_Fclose
#endif

/*
 * default memory allocator for libavutil
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * default memory allocator for libavutil
 */


#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "mem_t.h"



/* You can redefine av_malloc and av_free in your project to use your
   memory allocator. You do not need to suppress this file because the
   linker will do it automatically. */

void *av_malloc_t(unsigned int size)
{
    void *ptr = NULL;

    /* let's disallow possible ambiguous cases */
    //printf("av_malloc_t %d\n",size);
    ptr = malloc(size);
    return ptr;
}

void *av_realloc_t(void *ptr, unsigned int size)
{
    //printf("av_realloc_t %d\n",size);
    return realloc(ptr, size);
}

void av_free_t(void *ptr)
{
    /* XXX: this test should not be needed on most libcs */
    if (ptr)
        free(ptr);
}

void av_freep_t(void *arg)
{
    void **ptr= (void**)arg;
    av_free_t(*ptr);
    *ptr = NULL;
}

void *av_mallocz_t(unsigned int size)
{
    void *ptr = av_malloc_t(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

char *av_strdup_t(const char *s)
{
    char *ptr= NULL;
    if(s){
        int len = strlen(s) + 1;
        ptr = av_malloc_t(len);
        if (ptr)
            memcpy(ptr, s, len);
    }
    return ptr;
}


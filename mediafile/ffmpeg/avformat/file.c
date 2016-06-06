/*
 * Buffered file io for ffmpeg system
 * Copyright (c) 2001 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
//#define _GNU_SOURCE //define O_DIRECT

#ifdef CONFIG_WIN32
#include "stdafx.h"
#endif
#include "common.h"
#include "avio.h"
#include <fcntl.h>

#ifndef CONFIG_WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#else
#include <io.h>
#define open(fname,oflag,pmode) _open(fname,oflag,pmode)
#endif /* CONFIG_WIN32 */

#include "berrno.h"

/**
 * Return TRUE if val is a prefix of str. If it returns TRUE, ptr is
 * set to the next character in 'str' after the prefix.
 *
 * @param str input string
 * @param val prefix to test
 * @param ptr updated after the prefix in str in there is a match
 * @return TRUE if there is a match
 */
int strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}


/* standard file protocol */

static int file_open(URLContext *h, const char *filename, int flags)
{
    int access;
    int fd;

    strstart(filename, "file:", &filename);

    if (flags & URL_RDWR) {
        access = O_CREAT | O_TRUNC | O_RDWR;
    } else if (flags & URL_WRONLY) {
        access = O_CREAT | O_TRUNC | O_WRONLY;
    } else {
        access = O_RDONLY;
    }
#if defined(CONFIG_WIN32) || defined(CONFIG_OS2) || defined(__CYGWIN__)
    access |= O_BINARY;
#else
	access |= O_LARGEFILE ;//|O_SYNC;//|O_DIRECT;

#endif

    //fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, S_IWUSR);
    fd = open(filename, access, 0666);
    if (fd < 0)
        return -ENOENT;
    h->priv_data = (void *)(size_t)fd;
	//printf("file_open fd %d\n",fd);
    return 0;
}

static int file_read(URLContext *h, unsigned char *buf, int size)
{
	int ret = 0;
    int fd = (size_t)h->priv_data;
    ret = read(fd, buf, size);
//printf("%s line %d size %d,ret %d\n",__FUNCTION__,__LINE__,size,ret);	

	return ret;
}

static int file_write(URLContext *h, unsigned char *buf, int size)
{
	int ret = 0;
    int fd = (size_t)h->priv_data;
	
	//printf("%s line %d fd %d size %d\n",__FUNCTION__,__LINE__,fd,size);	
    ret= write(fd, buf, size);
	//ret= size;
//printf("%s line %d ret %d##############\n",__FUNCTION__,__LINE__,ret);	
	return ret;
}

/* XXX: use llseek */
static offset_t file_seek(URLContext *h, offset_t pos, int whence)
{
    int fd = (size_t)h->priv_data;
	
//printf("%s line %d pos %lld,whence %d\n",__FUNCTION__,__LINE__,pos,whence);	

#if defined(CONFIG_WIN32) && !defined(__CYGWIN__)
    return _lseeki64(fd, pos, whence);
#else
    return lseek64(fd, pos, whence);
#endif
}

static int file_close(URLContext *h)
{
    int fd = (size_t)h->priv_data;
    return close(fd);
}

URLProtocol file_protocol = {
    "file",
    file_open,
    file_read,
    file_write,
    file_seek,
    file_close,
	NULL,
};

/* pipe protocol */

static int pipe_open(URLContext *h, const char *filename, int flags)
{
    int fd;

    if (flags & URL_WRONLY) {
        fd = 1;
    } else {
        fd = 0;
    }
#if defined(CONFIG_WIN32) || defined(CONFIG_OS2) || defined(__CYGWIN__)
    setmode(fd, O_BINARY);
#endif
    h->priv_data = (void *)(size_t)fd;
    h->is_streamed = 1;
    return 0;
}

static int pipe_read(URLContext *h, unsigned char *buf, int size)
{
    int fd = (size_t)h->priv_data;
    return read(fd, buf, size);
}

static int pipe_write(URLContext *h, unsigned char *buf, int size)
{
    int fd = (size_t)h->priv_data;
    return write(fd, buf, size);
}

static int pipe_close(URLContext *h)
{
    return 0;
}

URLProtocol pipe_protocol = {
    "pipe",
    pipe_open,
    pipe_read,
    pipe_write,
    NULL,
    pipe_close,
};

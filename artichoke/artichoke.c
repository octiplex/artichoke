//
//  artichoke.c
//  artichoke
//
//  Created by Nicolas BACHSCHMIDT on 10/12/2012.
//  Copyright (c) 2012 Octiplex. All rights reserved.
//

#include "artichoke.h"

#include <limits.h>
#include <mach-o/fat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct artichoke_opaque {
    artichoke_fd_t fd;
    off_t offset, size;
    uint32_t retain_count;
    void (^cleanup_handler)(void);
};

artichoke_t artichoke_create(artichoke_fd_t fd, off_t offset, off_t size, void (^cleanup_handler)(void))
{
    if ( ! fd ) {
        return NULL;
    }
    
    artichoke_t archive = malloc(sizeof(struct artichoke_opaque));
    archive->fd = fd;
    archive->offset = offset;
    archive->size = size;
    archive->retain_count = 1;
    archive->cleanup_handler = cleanup_handler ? Block_copy(cleanup_handler) : NULL;
    
    
    return archive;
}

void artichoke_retain(artichoke_t archive)
{
    archive->retain_count++;
}

void artichoke_release(artichoke_t archive)
{
    if ( ! -- archive->retain_count ) {
        
        if ( archive->cleanup_handler ) {
            archive->cleanup_handler();
            Block_release(archive->cleanup_handler);
        }

        free(archive);
    }
}

bool artichoke_enumarate_archives(artichoke_fd_t fd, void (^block)(artichoke_t archive, bool *stop))
{
    union {
        struct fat_header fat_header;
        char ar_magic[SARMAG];
    } header;
    
    if ( 0 > lseek(fd, 0, SEEK_SET) )
        return false;
    
    if ( sizeof(header) != read(fd, &header, sizeof(header)) )
        return false;
    
    bool stop = false;
    
    if ( ! memcmp(header.ar_magic, ARMAG, SARMAG) )
    {
        artichoke_t archive = artichoke_create(fd, 0, lseek(fd, 0, SEEK_END), NULL);
        if ( ! artichoke_validate(archive) ) {
            artichoke_release(archive);
            return false;
        }
        block(archive, &stop);
        artichoke_release(archive);
        return true;
    }
    
    if ( FAT_MAGIC != OSSwapBigToHostInt32(header.fat_header.magic) ) {
        return false;
    }
    
    uint32_t n = OSSwapBigToHostInt32(header.fat_header.nfat_arch);
    
    for ( uint32_t i = 0; i < n; i++ )
    {
        struct fat_arch fat_arch;
        
        if ( 0 >= lseek(fd, sizeof(header.fat_header) + sizeof(fat_arch) * i, SEEK_SET) )
            return false;
        
        if ( sizeof(fat_arch) != read(fd, &fat_arch, sizeof(fat_arch)) )
            return false;
        
        artichoke_t archive = artichoke_create(fd, OSSwapBigToHostInt32(fat_arch.offset), OSSwapBigToHostInt32(fat_arch.size), NULL);
        if ( ! artichoke_validate(archive) ) {
            artichoke_release(archive);
            return false;
        }
        block(archive, &stop);
        artichoke_release(archive);
        if ( stop )
            break;
    }
    
    return true;
}

bool artichoke_enumerate_files(artichoke_t archive, void (^block)(off_t offset, bool *stop))
{
    __block bool stop = false;
    off_t offset = SARMAG;
    
    do {
        if ( artichoke_is_eof(archive, offset) ) {
            return true;
        }
        
        struct ar_hdr header;
        
        if ( ! artichoke_read_header(archive, offset, &header) ) {
            return false;
        }
        
        size_t size;
        if ( ! artichoke_get_size(&header, &size) ) {
            return false;
        }
        
        block(offset, &stop);
        
        offset += sizeof(header) + size;
    }
    while ( ! stop );
    
    return true;
}

bool artichoke_read_header(artichoke_t archive, off_t offset, struct ar_hdr *header)
{
    if ( ! artichoke_read(archive, offset, sizeof(*header), header) ) {
        return false;
    }
    
    if ( memcmp(header->ar_fmag, ARFMAG, strlen(ARFMAG)) ) {
        return false;
    }
    return true;
}

bool artichoke_write_header(artichoke_t archive, off_t offset, const struct ar_hdr *header)
{
    if ( ! artichoke_write(archive, offset, sizeof(*header), header) ) {
        return false;
    }
    return true;
}

bool artichoke_get_date(struct ar_hdr const *header, time_t *date)
{
    unsigned long long integer;
    if ( ! artichoke_get_integer(header->ar_date, sizeof(header->ar_date), &integer) ) {
        return false;
    }
    *date = integer;
    return true;
}

bool artichoke_set_date(struct ar_hdr *header, time_t date)
{
    return artichoke_set_integer(header->ar_date, sizeof(header->ar_date), date);
}

bool artichoke_get_size(struct ar_hdr const *header, size_t *size)
{
    unsigned long long integer;
    if ( ! artichoke_get_integer(header->ar_size, sizeof(header->ar_size), &integer) ) {
        return false;
    }
    *size = integer;
    return true;
}

bool artichoke_get_integer(const char *header_field, size_t length, unsigned long long *integer)
{
    if ( length > 20 )
        return false;
    
    char str[21];
    memset(str, 0, sizeof(str));
    memmove(str, header_field, length);
    
    char *end;
    unsigned long long n = strtoull(str, &end, 10);
    if ( n == ULLONG_MAX ) {
        return false;
    }
    
    if ( end == str ) {
        return false;
    }
    
    while ( *end ) {
        if ( *(end++) != ' ' ) {
            return false;
        }
    }
    *integer = n;
    return true;
}

bool artichoke_set_integer(char *header_field, size_t length, unsigned long long integer)
{
    if ( length > 20 )
        return false;
    
    char str[21];
    int len = snprintf(str, 21, "%llu", integer);
    if ( len > length ) {
        return false;
    }
    
    memmove(header_field, str, len);
    while ( len < length ) {
        header_field[len++] = ' ';
    }
    
    return true;
}

bool artichoke_update(artichoke_t archive)
{
    __block bool is_first = true;
    __block time_t max_date = 0;
    __block bool has_error = false;
    bool done;
    
    done = artichoke_enumerate_files(archive, ^(off_t offset, bool *stop) {
        
        if ( is_first ) {
            is_first = false;
            return;
        }
        
        struct ar_hdr header;
        time_t date;
        
        if ( ! artichoke_read_header(archive, offset, &header) || ! artichoke_get_date(&header, &date) ) {
            *stop = has_error = true;
            return;
        }
        
        if ( date > max_date ) {
            max_date = date;
        }
    });
    
    if ( ! done || ! max_date )
        return false;
    
    done = artichoke_enumerate_files(archive, ^(off_t offset, bool *stop) {
        
        struct ar_hdr header;
        
        if ( ! artichoke_read_header(archive, offset, &header) || ! artichoke_set_date(&header, max_date) || ! artichoke_write_header(archive, offset, &header) ) {
            *stop = has_error = true;
            return;
        }
        
        *stop = true;
        
    });
    
    return done ? ! has_error : false;
}

bool artichoke_validate(artichoke_t archive)
{
    char mag[SARMAG];
    
    if ( ! artichoke_read(archive, 0, sizeof(mag), mag) || memcmp(mag, ARMAG, sizeof(mag)) )
        return false;
    
    return true;
}

bool _artichoke_seek(artichoke_t archive, off_t offset, size_t size)
{
    return offset >= 0 && offset + size <= archive->size && lseek(archive->fd, archive->offset + offset, SEEK_SET) >= 0;
}

bool artichoke_read(artichoke_t archive, off_t offset, size_t size, void *buffer)
{
    if ( ! _artichoke_seek(archive, offset, size) )
        return false;
    
    ssize_t n = read(archive->fd, buffer, size);
    return n == size;
}

bool artichoke_write(artichoke_t archive, off_t offset, size_t size, const void *buffer)
{
    if ( ! _artichoke_seek(archive, offset, size) )
        return false;
    
    ssize_t n = write(archive->fd, buffer, size);
    return n == size;
}

bool artichoke_is_eof(artichoke_t archive, off_t offset)
{
    return offset == archive->size;
}

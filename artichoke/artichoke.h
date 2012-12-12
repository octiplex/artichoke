//
//  artichoke.h
//  artichoke
//
//  Created by Nicolas BACHSCHMIDT on 10/12/2012.
//  Copyright (c) 2012 Octiplex. All rights reserved.
//

#ifndef _ARTICHOKE_H_
#define _ARTICHOKE_H_

#include <ar.h>
#include <Block.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

typedef struct artichoke_opaque *artichoke_t;
typedef int artichoke_fd_t;

artichoke_t artichoke_create(artichoke_fd_t fd, off_t offset, off_t size, void (^cleanup_handler)(void));
void artichoke_retain(artichoke_t archive);
void artichoke_release(artichoke_t archive);

bool artichoke_enumarate_archives(artichoke_fd_t fd, void (^block)(artichoke_t archive, bool *stop));

bool artichoke_enumerate_files(artichoke_t archive, void (^block)(off_t offset, bool *stop));

bool artichoke_read_header(artichoke_t archive, off_t offset, struct ar_hdr *header);
bool artichoke_write_header(artichoke_t archive, off_t offset, const struct ar_hdr *header);

bool artichoke_get_date(const struct ar_hdr *header, time_t *date);
bool artichoke_set_date(struct ar_hdr *header, time_t date);

bool artichoke_get_size(const struct ar_hdr *header, size_t *size);

bool artichoke_get_integer(const char *header_field, size_t length, unsigned long long *integer);
bool artichoke_set_integer(char *header_field, size_t length, unsigned long long integer);

bool artichoke_validate(artichoke_t archive);
bool artichoke_update(artichoke_t archive);

bool artichoke_read(artichoke_t archive, off_t offset, size_t size, void *buffer);
bool artichoke_write(artichoke_t archive, off_t offset, size_t size, const void *buffer);
bool artichoke_is_eof(artichoke_t archive, off_t offet);

#endif // _ARTICHOKE_H_

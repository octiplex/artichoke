//
//  main.c
//  artichoke
//
//  Created by Nicolas BACHSCHMIDT on 10/12/2012.
//  Copyright (c) 2012 Octiplex. All rights reserved.
//

#include "artichoke.h"

#include <stdlib.h>
#include <sys/fcntl.h>
#include <unistd.h>

int main(int argc, const char * argv[])
{
    if ( argc != 2 ) {
        fprintf(stderr, "usage:\nartichoke file\n");
        exit(-1);
    }
    
    const char *filename = argv[1];
    artichoke_fd_t fd = open(filename, O_RDWR);
    if ( 0 > fd ) {
        fprintf(stderr, "could not open archive: \"%s\"\n", filename);
        exit(-1);
    }
    
    bool done = artichoke_enumarate_archives(fd, ^(artichoke_t archive, bool *stop) {
                
        if ( ! artichoke_update(archive) ) {
            fprintf(stderr, "could not update archive: \"%s\"\n", filename);
            exit(-1);
        }
    });
    
    if ( ! done ) {
        fprintf(stderr, "not a valid archive: \"%s\"\n", filename);
        exit(-1);
    }    
    
    close(fd);
    return 0;
}


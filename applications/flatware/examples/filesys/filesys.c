#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

int testWrite(void);
int testRead(void);

int main() {
    if ( testWrite() != 0 ) {
        return -1;
    }

    if ( testRead() != 0 ) {
        return -1;
    }

    return 0;
}

int testWrite(void) { 
    int fd = open( "fixpoint.txt", O_RDONLY );
    if ( fd < 0 ) {
        return -1;
    }

    char buf[] = "Hello, World!";
    write( fd, buf, sizeof(buf) );
    close( fd );

    return 0;
}

int testRead(void) {
    char buf[14];
    
    int fd = open( "fixpoint.txt", O_RDONLY );
    if ( fd < 0 ) {
        return -1;
    }

    int bytesRead = read( fd, buf, sizeof(buf) );
    close( fd );

    if ( bytesRead != sizeof(buf) ) {
        return -1;
    }

    char expected[] = "Hello, World!";
    if (strncmp( buf, expected, 13 ) != 0 ) {
        return -1;
    }

    return 0;
}

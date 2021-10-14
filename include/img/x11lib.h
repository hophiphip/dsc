#ifndef __IMG_X11LIB
#define __IMG_X11LIB

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <stdlib.h>  // exit
#include <stdio.h>   // fprintf
#include <string.h>  // memory
#include <assert.h>  // assert

#define BITS_IN_BYTE 8

// R - 1 byte, 
// G - 1 byte, 
// B - 1 byte
#define BYTES_IN_RGB 3

// No malloc for now
#define ALLOC_IMG_BUFFER()                                     \
    unsigned char __x11lib_local_img_buffer[img_byte_count()]; \
    img_buffer = __x11lib_local_img_buffer


// Function type for `img_buffer_transform` to handle
// image transformation / (down)scaling
typedef void (*transform_func)(
        char         *pixels, // image pixel buffer 
        const int      width, // image width
        const int     height, // image height
        const int byte_count  // image pixel byte count
);

// Module globals
static Display              *display;
static Window                   root;
static XShmSegmentInfo       shminfo;
static Screen                *screen;
XImage                       *ximage;
XWindowAttributes attributes = { 0 };

unsigned char *img_buffer = NULL;
const int downscale_coef  = 2;

int   img_width()          { return attributes.width;  }
int   img_height()         { return attributes.height; }
int   img_bits_per_pixel() { return ximage->bits_per_pixel; }
int   img_byte_count()     { return img_width() * img_height() * img_bits_per_pixel() / BITS_IN_BYTE; }
char* img_pixels()         { return ximage->data; }


int  img_buffer_width()      { return img_width()  / downscale_coef; }
int  img_buffer_height()     { return img_height() / downscale_coef; }
int  img_buffer_byte_count() { return img_buffer_width() * img_buffer_height() * BYTES_IN_RGB; }
void img_buffer_update();


void xlib_init();
void xlib_cleanup();

#endif // __IMG_X11LIB

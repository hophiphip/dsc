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
#define img_mgr_buffer_alloc(mgr)                                  \
    unsigned char __x11lib_local_img_buffer[img_byte_count(&mgr)]; \
    mgr.img_buffer = __x11lib_local_img_buffer

// TODO: turn `img_buffer_transform` into example function for downscaling with coef 2
// and make this type as a fleld for `img_mgr` structure
// Function type for `img_buffer_transform` to handle
// image transformation / (down)scaling
typedef void (*transform_func)(
    unsigned char *pixels, // image (RGB) pixel buffer
    const int width,       // image width
    const int height,      // image height
    const int byte_count   // image byte count: basically: width * height * 3 // for 3 RGB bytes
);

typedef struct {
    // X11 Lib related
    Window                window;
    Screen               *screen;
    Display             *display;
    XShmSegmentInfo      shminfo;

    // Screen image related
    XImage               *ximage;
    XWindowAttributes attributes;

    // XImage `data` field related
    char             *img_pixels;
    int                img_width;
    int               img_height;
    int       img_bits_per_pixel;
    int           img_byte_count;
   
    // Image buffer related
    unsigned char    *img_buffer;
    int           downscale_coef;
    int         img_buffer_width;
    int        img_buffer_height;
    int    img_buffer_byte_count;

    // Transformation function 
    // that is performed on image buffer pixels on each update
    //  Example: donscaling function
    transform_func *img_buffer_transformation;
} img_mgr;

// TODO: better not use macros / `inline` too doesn't seem like a good alternative
// TODO: Can possibly be moved to `img_mgr`
#define img_width(mgr) ((mgr)->attributes.width)
#define img_height(mgr) ((mgr)->attributes.height)
#define img_bits_per_pixel(mgr) ((mgr)->ximage->bits_per_pixel)
#define img_pixels(mgr) ((mgr)->ximage->data)
#define img_byte_count(mgr) ((img_width(mgr)) * (img_height(mgr)) * (img_bits_per_pixel(mgr)) / BITS_IN_BYTE)

#define img_buffer_width(mgr) ((img_width(mgr)) / (mgr)->downscale_coef)
#define img_buffer_height(mgr) ((img_height(mgr)) / (mgr)->downscale_coef)
#define img_buffer_byte_count(mgr) ((img_buffer_width(mgr)) * (img_buffer_height(mgr)) * BYTES_IN_RGB)


void img_mgr_init(img_mgr *mgr);
void img_mgr_buffer_update(img_mgr *mgr); 
void img_mgr_free(img_mgr *mgr);

#endif // __IMG_X11LIB

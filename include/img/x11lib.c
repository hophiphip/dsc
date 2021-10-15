#include "x11lib.h"

// TODO: Turn this into example function and put it in to main.c / utils.c (create a file for utilities)
static void img_buffer_transform(img_mgr *mgr) {
    // TODO: make a proper NULL handling
    assert(mgr);
    // TODO: Handle case with downscale_coef == 1 mb. use enum instead
    assert(mgr->downscale_coef >= 1);

    int width  = img_width(mgr), 
        height = img_height(mgr),
        bpp    = img_bits_per_pixel(mgr),
        image_pixel_idx = 0;
    
    unsigned int r = 0, g = 0, b = 0;

    // NOTE: This is important, as ximage in Shm implementation gets updated in real time
    // so ximage->data buffer might be updated during image downscaling operation
    memcpy(mgr->img_buffer, img_pixels(mgr), img_byte_count(mgr));

    for (int y = 0; y < height; y += mgr->downscale_coef) {
        for (int x = 0; x < width; x += mgr->downscale_coef) {
            int i = (y * width + x) * (bpp / 8);

            r = 0, g = 0, b = 0;
            for (int h = 0; h < mgr->downscale_coef; ++h) {
                for (int v = 0; v < mgr->downscale_coef; ++v) {
                    r += (unsigned int)mgr->img_buffer[i + 2 + (v * bpp / 8) + (h * width * bpp / 8)];
                    g += (unsigned int)mgr->img_buffer[i + 1 + (v * bpp / 8) + (h * width * bpp / 8)];
                    b += (unsigned int)mgr->img_buffer[i     + (v * bpp / 8) + (h * width * bpp / 8)];
                }
            }
            
            mgr->img_buffer[image_pixel_idx    ] = (unsigned char)(r / (mgr->downscale_coef * mgr->downscale_coef)) & 0xff;
            mgr->img_buffer[image_pixel_idx + 1] = (unsigned char)(g / (mgr->downscale_coef * mgr->downscale_coef)) & 0xff;
            mgr->img_buffer[image_pixel_idx + 2] = (unsigned char)(b / (mgr->downscale_coef * mgr->downscale_coef)) & 0xff;
            image_pixel_idx += 3;
        }
    }
}

// Update image buffer
void img_mgr_buffer_update(img_mgr *mgr) 
{
    if (mgr) {
        // Update image
        XShmGetImage(
            mgr->display,    // Display 
            mgr->window,       // Drawable
            mgr->ximage,     // XImage
            0,          // x offset 
            0,          // y offset
            0x00ffffff  // plane mask (which planes to read)
        );

        // TODO: THis function must be called from mgr structure if != `NULL` on screen pixels
        img_buffer_transform(mgr);
    }
}


// TODO: Create a fallback for case where Shm module is not supported
void img_mgr_init(img_mgr *mgr)
{
    // TODO: Add null check
    assert(mgr);

    mgr->img_buffer = NULL;
    mgr->downscale_coef = 2;

    mgr->display = XOpenDisplay(NULL);
    mgr->window = DefaultRootWindow(mgr->display);

    if (XShmQueryExtension(mgr->display) != True) {
        fprintf(stderr, "shared memory extension is not supported\n");
        exit(1);
    }

    XGetWindowAttributes(mgr->display, mgr->window, &(mgr->attributes));

    mgr->screen = mgr->attributes.screen;

    mgr->ximage = XShmCreateImage(
            mgr->display,                        // Display
            DefaultVisualOfScreen(mgr->screen),  // Visual
            DefaultDepthOfScreen(mgr->screen),   // depth
            ZPixmap,                             // format
            NULL,                                // data
            &(mgr->shminfo),                     // ShmSegmentInfo
            mgr->attributes.width,               // width
            mgr->attributes.height               // height
    );

    mgr->shminfo.shmid = shmget(IPC_PRIVATE, mgr->ximage->bytes_per_line * mgr->ximage->height, IPC_CREAT|0777);
    mgr->shminfo.shmaddr = mgr->ximage->data = (char *)shmat(mgr->shminfo.shmid, 0, 0);
    mgr->shminfo.readOnly = False;

    if (mgr->shminfo.shmid < 0) {
        fprintf(stderr, "fatal shminfo error\n");
        exit(1);
    }

    Status status = XShmAttach(mgr->display, &(mgr->shminfo));
    if (!status) {
        fprintf(stderr, "shm attach failed\n");
        exit(1);
    }
}

void img_mgr_free(img_mgr *mgr)
{
    if (mgr) {
        XShmDetach(mgr->display, &(mgr->shminfo));
        XDestroyImage(mgr->ximage);
        shmdt(mgr->shminfo.shmaddr);

        XCloseDisplay(mgr->display);
    }
}



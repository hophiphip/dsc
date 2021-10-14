#include "x11lib.h"

static void img_buffer_transform() {
    // TODO: Handle case with downscale_coef == 1 mb. use enum instead
    assert(downscale_coef >= 1);

    int width  = img_width(), 
        height = img_height(),
        bpp    = img_bits_per_pixel(),
        image_pixel_idx = 0;
    
    unsigned int r = 0, g = 0, b = 0;

    // NOTE: This is important, as ximage in Shm implementation gets updated in real time
    // so ximage->data buffer might be updated during image downscaling operation
    memcpy(img_buffer, img_pixels(), img_byte_count());

    for (int y = 0; y < height; y += downscale_coef) {
        for (int x = 0; x < width; x += downscale_coef) {
            int i = (y * width + x) * (bpp / 8);

            r = 0, g = 0, b = 0;
            for (int h = 0; h < downscale_coef; ++h) {
                for (int v = 0; v < downscale_coef; ++v) {
                    r += (unsigned int)img_buffer[i + 2 + (v * bpp / 8) + (h * width * bpp / 8)];
                    g += (unsigned int)img_buffer[i + 1 + (v * bpp / 8) + (h * width * bpp / 8)];
                    b += (unsigned int)img_buffer[i     + (v * bpp / 8) + (h * width * bpp / 8)];
                }
            }
            
            img_buffer[image_pixel_idx    ] = (unsigned char)(r / (downscale_coef * downscale_coef)) & 0xff;
            img_buffer[image_pixel_idx + 1] = (unsigned char)(g / (downscale_coef * downscale_coef)) & 0xff;
            img_buffer[image_pixel_idx + 2] = (unsigned char)(b / (downscale_coef * downscale_coef)) & 0xff;
            image_pixel_idx += 3;
        }
    }
}

// Update image buffer
void img_buffer_update() 
{
    // Update image
    XShmGetImage(
        display,    // Display 
        root,       // Drawable
        ximage,     // XImage
        0,          // x offset 
        0,          // y offset
        0x00ffffff  // plane mask (which planes to read)
    );

    img_buffer_transform();
}


// TODO: Create a fallback for case where Shm module is not supported
void xlib_init()
{
    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);

    if (XShmQueryExtension(display) != True) {
        fprintf(stderr, "shared memory extension is not supported\n");
        exit(1);
    }

    XGetWindowAttributes(display, root, &attributes);

    screen = attributes.screen;

    ximage = XShmCreateImage(
            display,                        // Display
            DefaultVisualOfScreen(screen),  // Visual
            DefaultDepthOfScreen(screen),   // depth
            ZPixmap,                        // format
            NULL,                           // data
            &shminfo,                       // ShmSegmentInfo
            attributes.width,               // width
            attributes.height               // height
    );

    shminfo.shmid = shmget(IPC_PRIVATE, ximage->bytes_per_line * ximage->height, IPC_CREAT|0777);
    shminfo.shmaddr = ximage->data = (char *)shmat(shminfo.shmid, 0, 0);
    shminfo.readOnly = False;

    if (shminfo.shmid < 0) {
        fprintf(stderr, "fatal shminfo error\n");
        exit(1);
    }

    Status status = XShmAttach(display, &shminfo);
    if (!status) {
        fprintf(stderr, "shm attach failed\n");
        exit(1);
    }
}

void xlib_cleanup()
{
    XShmDetach(display, &shminfo);
    XDestroyImage(ximage);
    shmdt(shminfo.shmaddr);

    XCloseDisplay(display);
}



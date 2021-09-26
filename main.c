#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <assert.h>

#include <time.h>
#define FPS(start) (CLOCKS_PER_SEC / (clock() - start))

#include "mongoose/mongoose.h"

char *src_url = "ws://localhost:8080/ws";
bool is_connected = false;
bool is_streaming = false;

const char *dots[] = {
    ".  ", ".. ", "..."
};

const int resize_coef = 3;

// TODO: Rename it to image config
// Do not store pixels here --> they can be accesed from ximage buffer
typedef struct {
    uint8_t *pixels;
    int width;
    int height;
    int bits_per_pixel;
    int byte_size;
} sc_image;

struct sc_image_conf {
    int width;
    int height;
    int bits_per_pixel;
    int byte_size;

    Display *display;
    Drawable *drawable;
    XImage *img;

    struct mg_mgr *mgr;
};

int ppm_write(const char *file_path, const sc_image *sc) 
{
    const unsigned long ppm_max_color_component = 256UL - 1UL; 
    const char *ppm_comment                     = "# screen capture output";
    const char *ppm_format                      = "P6";

    if (file_path && sc) {
        FILE *fp;

        if ((fp = fopen(file_path, "wb")) == NULL) {
            fprintf(stderr, "failed to open file: %s\n", file_path);
            return 1;
        }

        // Log ppm image header
        fprintf(stdout, "PPM Header:\n\t%s\n\t%s\n\t%d\n\t%d\n\t%lu\n", 
                ppm_format, 
                ppm_comment, 
                sc->width / resize_coef, 
                sc->height / resize_coef, 
                ppm_max_color_component
        );

        // Write header
        fprintf(fp, "%s\n %s\n %d\n %d\n %lu\n",
                ppm_format,
                ppm_comment, 
                sc->width / resize_coef, 
                sc->height / resize_coef, 
                ppm_max_color_component
        );

        int fwrite_count = 0, x, y;
        // Write image bytes and resize image
        // Slow implementation for now will be enough
        uint8_t rgb_pixels[3];
        for (y = 0; y < sc->height; y += resize_coef) {
            for (x = 0; x < sc->width; x += resize_coef) {
                int i = (y * sc->width + x) * (sc->bits_per_pixel / 8);

                // Resize image to lower resolution
                // r, b, g - is a sum of colors in square (resize_coef x resize_coef)
                uint32_t r = 0, g = 0, b = 0;
                for (int h = 0; h < resize_coef; ++h) {
                    for (int v = 0; v < resize_coef; ++v) {
                        r += (uint32_t)sc->pixels[i + 2 + (v * 4) + (h * sc->width * sc->bits_per_pixel / 8)];
                        g += (uint32_t)sc->pixels[i + 1 + (v * 4) + (h * sc->width * sc->bits_per_pixel / 8)];
                        b += (uint32_t)sc->pixels[i     + (v * 4) + (h * sc->width * sc->bits_per_pixel / 8)];
                    }
                }
                rgb_pixels[0] = (uint8_t)(r / (resize_coef * resize_coef)) & 0xff;
                rgb_pixels[1] = (uint8_t)(g / (resize_coef * resize_coef)) & 0xff;
                rgb_pixels[2] = (uint8_t)(b / (resize_coef * resize_coef)) & 0xff;

                fwrite(rgb_pixels, 1, 3, fp);
                ++fwrite_count;
            }
        }

        assert(fwrite_count == ((sc->width / resize_coef) * (sc->height / resize_coef)));

        fclose(fp);
        return 0;
    }

    return 1;
}


static void stream_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    switch (ev) {
        case MG_EV_ERROR: {
            LOG(LL_ERROR, ("%p %s", c->fd, (char *) ev_data));
        } break;

        case MG_EV_WS_OPEN: {
            c->label[0] = 'W';
            is_connected = true;
        } break;

        case MG_EV_WS_MSG: {
            /* DO NOTHING */
        } break;
    }

    if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
        is_connected = false;
    }
}

static void stream_timer_callback(void *arg) 
{
    struct sc_image_conf *conf = (struct sc_image_conf *)arg;
    static char dot_idx = 0;

    if (is_connected) {
        for (struct mg_connection *c = conf->mgr->conns; c != NULL; c = c->next) {
            if (c->label[0] == 'W') {
                // Update image
                XShmGetImage(
                    conf->display,          
                    *(conf->drawable),
                    conf->img,        
                    0,                
                    0,                
                    0x00ffffff        
                );


                mg_ws_send(c, conf->img->data, conf->byte_size, WEBSOCKET_OP_TEXT);
            }
        }
    } else {
        LOG(LL_INFO, ("Not connected%s", dots[dot_idx]));
        dot_idx = (dot_idx + 1) % 3;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        is_streaming = true;
    }

    Display *display = XOpenDisplay(NULL);
    // TODO: Create a fallback with standard xlib calls
    // for systems that do not support shared memory
    if (XShmQueryExtension(display) != True) {
        fprintf(stderr, "shared memory extension is not supported\n");
        exit(1);
    }

    Window root = DefaultRootWindow(display);
    sc_image sc = { 0 };

    XWindowAttributes attributes = { 0 };
    XGetWindowAttributes(display, root, &attributes);

    Screen *screen = attributes.screen;
    XShmSegmentInfo shminfo;

    XImage* ximage = XShmCreateImage(
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

    // sc initialization --------------------------------------------
    sc.width  = attributes.width;
    sc.height = attributes.height;

    XShmGetImage(
        display,          // Display 
        root,             // Drawable
        ximage,           // XImage
        0,                // x offset 
        0,                // y offset
        0x00ffffff        // plane mask (which planes to read)
    );

    sc.bits_per_pixel = ximage->bits_per_pixel; 
    sc.byte_size      = sc.width  *
                            sc.height *
                               sc.bits_per_pixel / 8;

    uint8_t screen_buffer[sc.byte_size];
    sc.pixels = screen_buffer;

    memcpy(sc.pixels, ximage->data, sc.byte_size);

    // -------------------------------------------------------------- 

    // Log sc image info:
    if (sc.width && sc.height) {
        fprintf(stdout, "INFO:\n\twidth: %d\n\theight: %d\n\tbits per pixel: %d\n\tbuffer size: %d bytes\n",
            sc.width,
            sc.height,
            sc.bits_per_pixel,
            sc.byte_size
        );
    } else {
        fprintf(stderr, "Failed to get window attributes\n");
        exit(1);
    }   

    if (!is_streaming) {
        if (ppm_write("test.ppm", &sc) == 0)
            fprintf(stdout, "PPM Write status: OK\n");
        else 
            fprintf(stderr, "PPM Write status: Error\n");
    }
    else {
        struct mg_mgr mgr;
        struct mg_timer timer;
        struct sc_image_conf img_conf = { 
            .width = attributes.width,
            .height = attributes.height,
            .bits_per_pixel = ximage->bits_per_pixel,
            .byte_size = attributes.width * attributes.height * (ximage->bits_per_pixel / 8),
            
            .img = ximage,
            .display = display,
            .drawable = &root,

            .mgr = NULL
        };
        struct mg_connection *conn;

        mg_log_set("3");
        mg_mgr_init(&mgr);
        conn = mg_ws_connect(&mgr, src_url, stream_handler, &img_conf, NULL);
        if (conn == NULL) {
            fprintf(stderr, "could not connect to the server\n");
            mg_mgr_free(&mgr);
            goto cleanup;
        }

        img_conf.mgr = &mgr;

        mg_timer_init(&timer, 1000, MG_TIMER_REPEAT, stream_timer_callback, &img_conf);
        for (;;)
            mg_mgr_poll(&mgr, 1000);

        mg_timer_free(&timer);
        mg_mgr_free(&mgr);
    }

cleanup:
    // Cleanup
    XShmDetach(display, &shminfo);
    XDestroyImage(ximage);
    shmdt(shminfo.shmaddr);

    XCloseDisplay(display);
}

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

#include "mongoose/mongoose.h"

typedef void (*transform_func)(void);

// App can either dump image or stream it
enum app_mode_t {
    DUMP_MODE = 1,
    STREAM_MODE,
    MODE_LEN,    
};

// App mode related 
enum app_mode_t app_mode = DUMP_MODE;
char *dump_file_path = NULL; // example: test.ppm
char *stream_src_url = NULL; // example: ws://localhost:8080/ws
bool is_connected = false;

// Globals
Display             *display;
Window                  root;
XShmSegmentInfo      shminfo;
Screen               *screen;
XImage               *ximage;
XWindowAttributes attributes = { 0 };

char *img_buffer = NULL;

const int downscale_coef = 2;


static inline int img_width() {
    return attributes.width;
}

static inline int img_height() {
    return attributes.height;
}

static inline int img_bits_per_pixel() {
    return ximage->bits_per_pixel;
}

static inline int img_byte_count() {
    return img_width() 
            * img_height() 
            * img_bits_per_pixel()
            / 8;
}

static inline char* img_pixels() {
    return ximage->data;
}

static inline int img_buffer_width() {
    return img_width() / downscale_coef;
}

static inline int img_buffer_height() {
    return img_height() / downscale_coef;
}

static inline int img_buffer_byte_count() {
    return img_buffer_width() 
            * img_buffer_height()
            * 3; // (r, g, b)
}

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
            
            img_buffer[image_pixel_idx    ] = (char)(r / (downscale_coef * downscale_coef)) & 0xff;
            img_buffer[image_pixel_idx + 1] = (char)(g / (downscale_coef * downscale_coef)) & 0xff;
            img_buffer[image_pixel_idx + 2] = (char)(b / (downscale_coef * downscale_coef)) & 0xff;
            image_pixel_idx += 3;
        }
    }
}

// Update image buffer
static void img_buffer_update() 
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

// Write image buffer bytes to a file
static int img_buffer_write(const char *path) 
{
    const unsigned long ppm_max_color_component = 256UL - 1UL; 
    const char *ppm_comment                     = "# screen capture output";
    const char *ppm_format                      = "P6";

    FILE *fp;
    if ((fp = fopen(path, "wb")) == NULL) {
        fprintf(stderr, "failed to open file: %s\n", path);
        return 1;
    }

    // Log ppm image header
    fprintf(stdout, "PPM Header:\n\t%s\n\t%s\n\t%d\n\t%d\n\t%lu\n", 
            ppm_format, 
            ppm_comment, 
            img_buffer_width(), 
            img_buffer_height(), 
            ppm_max_color_component
    );

    // Write header
    fprintf(fp, "%s\n %s\n %d\n %d\n %lu\n",
            ppm_format,
            ppm_comment, 
            img_buffer_width(), 
            img_buffer_height(), 
            ppm_max_color_component
    );
    
    fwrite(img_buffer, img_buffer_byte_count(), 1, fp);

    fclose(fp);
    return 0;
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


// NOTE: Mb. try simple HTTP for now instead of Websockets ?
static void stream_timer_callback(void *arg) 
{
    struct mg_mgr *mgr = (struct mg_mgr *)arg;

    if (is_connected) {
        for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
            if (c->label[0] == 'W') {
                img_buffer_update();
                mg_ws_send(c, img_buffer, img_buffer_byte_count(), WEBSOCKET_OP_BINARY);
            }
        }
    } else {
        LOG(LL_INFO, ("Not connected%s", ".."));
    }
}

void usage(int argc, char **argv) 
{
    assert(argc > 0);

    fprintf(stdout, 
    "Usage:\n\
    Take a screenshot and save it as `.ppm` image:\n\
        \t%s -i [output_image_name]\n\n\
    Stream current screen image to streaming server:\n\
        \t%s -s [streaming_server_url]\n", 
            
            argv[0], 
            argv[0]
    );
}

int main(int argc, char **argv)
{
    // Parse args 
    if (argc < 3) {
        usage(argc, argv);
        exit(1);
    }

    if (strcmp(argv[1], "-i") == 0) {
        app_mode = DUMP_MODE;
        dump_file_path = argv[2];
    } 
    else if (strcmp(argv[1], "-s") == 0) {
        app_mode = STREAM_MODE;
        stream_src_url = argv[2];
    } else {
        usage(argc, argv);
        exit(1);
    }

    // Initialization
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

    // No need for malloc for now
    char local_img_buffer[img_byte_count()];
    img_buffer = local_img_buffer;


    // Handle app mode
    //
    switch (app_mode) {
        case DUMP_MODE: {
            img_buffer_update();
            img_buffer_write(dump_file_path);
        } break;

        case STREAM_MODE: {
            struct mg_mgr mgr;
            struct mg_timer timer;
            struct mg_connection *conn;

            mg_log_set("3");
            mg_mgr_init(&mgr);
            conn = mg_ws_connect(&mgr, stream_src_url, stream_handler, &mgr, NULL);
            if (conn == NULL) {
                fprintf(stderr, "could not connect to: %s\n", stream_src_url);
                mg_mgr_free(&mgr);
            } else {
                mg_timer_init(&timer, 1000, MG_TIMER_REPEAT, stream_timer_callback, &mgr);
                for (;;)
                    mg_mgr_poll(&mgr, 1000);

                mg_timer_free(&timer);
                mg_mgr_free(&mgr);
            }
        } break;

        default:
            fprintf(stderr, "app mode was not set\n");

            // TODO: Handle cleanup properly
            XShmDetach(display, &shminfo);
            XDestroyImage(ximage);
            shmdt(shminfo.shmaddr);
            XCloseDisplay(display);

            exit(1);
    }


    // Cleanup
    XShmDetach(display, &shminfo);
    XDestroyImage(ximage);
    shmdt(shminfo.shmaddr);

    XCloseDisplay(display);
}

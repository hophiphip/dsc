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

const int resize_coef = 3;

typedef struct {
    uint8_t *pixels;
    int width;
    int height;
    int bits_per_pixel;
    int byte_size;
} sc_image;

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

        //// Write without resize
        //uint8_t rgb_pixels[3];
        ////for (int i = 0; i < sc->byte_size; i += 4) {
        //for (int y = 0; y < sc->height; y += 1)
        //    for (int x = 0; x < sc->width; x += 1) {
        //        //
        //        int i = (y * sc->width + x) * (sc->bits_per_pixel / 8); 

        //        // Without resize
        //        // BGR -> RGB
        //        //
        //        rgb_pixels[0] = (uint8_t)(sc->pixels[i + 2]); // R
        //        rgb_pixels[1] = (uint8_t)(sc->pixels[i + 1]); // G
        //        rgb_pixels[2] = (uint8_t)(sc->pixels[i    ]); // B
        //                                                      // A - ignored
        //        fwrite(rgb_pixels, 1, 3, fp);
        //    }
        //}


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

int main(int argc, char **argv)
{
    int is_streaming = 0;
    if (argc != 2) {
        // TODO: Add program params
        //fprintf(stdout, "Usage %s [OUTPUT PPM FILENAME]\n", argv[0]);
        //exit(1);
        is_streaming = 1;
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
        for (int i; is_streaming; ++i) {
            double start = clock();

            XShmGetImage(
                display,          // Display 
                root,             // Drawable
                ximage,           // XImage
                0,                // x offset 
                0,                // y offset
                0x00ffffff        // plane mask (which planes to read)
            );

            // work with: ximage->data  
            //
            
            if(!(i & 0x3F)) {
                fprintf(stdout, "\r[FPS]: %4.f", FPS(start));
                fflush(stdout);
            }
        }
    }

    // Cleanup
    XShmDetach(display, &shminfo);
    XDestroyImage(ximage);
    shmdt(shminfo.shmaddr);

    XCloseDisplay(display);
}

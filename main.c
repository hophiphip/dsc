#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
                sc->width, 
                sc->height, 
                ppm_max_color_component
        );

        // Write header
        fprintf(fp, "%s\n %s\n %d\n %d\n %lu\n",
                ppm_format,
                ppm_comment, 
                sc->width, 
                sc->height, 
                ppm_max_color_component
        );

        // Write image bytes
        uint8_t rgb_pixels[3];
        for (int i = 0; i < sc->byte_size; i += 4) {
            // BGR -> RGB
            rgb_pixels[0] = (uint8_t)(sc->pixels[i + 2] & 0xff); // R
            rgb_pixels[1] = (uint8_t)(sc->pixels[i + 1] & 0xff); // G
            rgb_pixels[2] = (uint8_t)(sc->pixels[i    ] & 0xff); // B
                                                                 // A - ignored
            fwrite(rgb_pixels, 1, 3, fp);
        }
        
        // Write all pixels (for later) 
        //fwrite(pixels, sc->byte_size, 1, fp);

        fclose(fp);
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stdout, "Usage %s [OUTPUT PPM FILENAME]\n", argv[0]);
        exit(1);
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
        fprintf(stdout, "INFO:\n\twidth: %d\n\theight: %d\n\tbits_per_pixel: %d\n\tbuffer size: %d bytes\n",
            sc.width,
            sc.height,
            sc.bits_per_pixel,
            sc.byte_size
        );
    } else {
        fprintf(stderr, "Failed to get window attributes\n");
        exit(1);
    }   

    if (ppm_write("test.ppm", &sc) == 0)
        fprintf(stdout, "PPM Write status: OK\n");
    else 
        fprintf(stderr, "PPM Write status: Error\n");
    
    // cleanup
    XShmDetach(display, &shminfo);
    XDestroyImage(ximage);
    shmdt(shminfo.shmaddr);

    XCloseDisplay(display);
}

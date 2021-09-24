#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>

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

        // TODO: Screenshots color is different (but it'll do for now)
        // Write image bytes
        uint8_t rgb_pixels[3];
        for (int i = 0; i < sc->byte_size; i += 4) {
            rgb_pixels[0] = (uint8_t)(sc->pixels[i    ] & 0xff); // R
            rgb_pixels[1] = (uint8_t)(sc->pixels[i + 1] & 0xff); // G
            rgb_pixels[2] = (uint8_t)(sc->pixels[i + 2] & 0xff); // B
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
    Window root = DefaultRootWindow(display);
    sc_image sc = { 0 };

    XWindowAttributes attributes = { 0 };
    XGetWindowAttributes(display, root, &attributes);

    // sc initialization --------------------------------------------
    sc.width  = attributes.width;
    sc.height = attributes.height;

    XImage* image = XGetImage(
        display,          // Display 
        root,             // Drawable
        0,                // x origin
        0,                // y origin
        sc.width,         // width of subimage
        sc.height,        // height of subimage
        AllPlanes,        // plane mask
        ZPixmap           // image format: ZPixmap (RGBA) | XYPixmap
    );

    sc.bits_per_pixel = image->bits_per_pixel; 
    sc.byte_size      = sc.width  *
                            sc.height *
                               sc.bits_per_pixel / 8;

    uint8_t screen_buffer[sc.byte_size];
    sc.pixels = screen_buffer;

    memcpy(sc.pixels, image->data, sc.byte_size);

    XDestroyImage(image);
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
    XCloseDisplay(display);
}

#include "img/x11lib.h"

#include "mongoose/mongoose.h"

// App can either dump image or stream it
enum app_mode_t {
    DUMP_MODE = 1,
    STREAM_MODE,
    MODE_LEN,    
};

img_mgr imgr;

// App mode related 
enum app_mode_t app_mode = DUMP_MODE;
char *dump_file_path = NULL; // example: test.ppm
char *stream_src_url = NULL; // example: ws://localhost:8080/ws
// 
bool is_connected    = false;

// Write image buffer bytes to a file
static int img_buffer_write(img_mgr *mgr, const char *path) 
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
            img_buffer_width(mgr), 
            img_buffer_height(mgr), 
            ppm_max_color_component
    );

    // Write header
    fprintf(fp, "%s\n %s\n %d\n %d\n %lu\n",
            ppm_format,
            ppm_comment, 
            img_buffer_width(mgr), 
            img_buffer_height(mgr), 
            ppm_max_color_component
    );
    
    fwrite(mgr->img_buffer, img_buffer_byte_count(mgr), 1, fp);

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


// NOTE: try simple HTTP for now instead of Websockets ?
static void stream_timer_callback(void *arg) 
{
    struct mg_mgr *mgr = (struct mg_mgr *)arg;

    if (is_connected) {
        for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
            if (c->label[0] == 'W') {
                img_buffer_update(&imgr);
                mg_ws_send(c, (char *)(imgr.img_buffer), img_buffer_byte_count(&imgr), WEBSOCKET_OP_BINARY);
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
        \t%s -i [output_image_name]\n\
    Example:\n\
      %s -i test.ppm\n\n\
    Stream current screen image to streaming server:\n\
        \t%s -s [streaming_server_url]\n\
    Example:\n\
      %s -s ws://localhost:8080/ws\n", 
            
            argv[0], 
            argv[0],
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

    imgr = xlib_init();
    ALLOC_IMG_BUFFER(imgr);

    // Handle app mode
    //
    switch (app_mode) {
        case DUMP_MODE: {
            img_buffer_update(&imgr);
            img_buffer_write(&imgr, dump_file_path);
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
            xlib_cleanup(&imgr);
            exit(1);
    }

    xlib_cleanup(&imgr);
}

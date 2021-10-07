
enum {
    STREAMING = 0,
    IMAGE_DUMP
} app_mode;

typedef struct {
    // App related
    enum app_mode mode;

    // Image related
    uint8_t *pixels;
    int      width;
    int      height;
    int      bits_per_pixel;
    int      byte_size;

    // XLib related
    Display  *display;
    Drawable *drawable;
    XImage   *img;

    // Web related
    struct mg_mgr *mgr;
} app_config;

app_config global_config = {
    .pixels         = NULL,
    .width          = 0,
    .height         = 0,
    .bits_per_pixel = 0,
    .byte_size      = 0,

    .display  = NULL,
    .drawable = NULL,
    .img      = NULL,

    .mgr = NULL,
};


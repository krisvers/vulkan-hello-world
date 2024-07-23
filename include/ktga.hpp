#ifndef KRISVERS_KTGA_HPP
#define KRISVERS_KTGA_HPP

struct ktga_header_t {
    unsigned char id_len;
    unsigned char color_map_type;
    unsigned char img_type;
    unsigned short color_map_origin;
    unsigned short color_map_length;
    unsigned char color_map_depth;
    unsigned short img_x_origin;
    unsigned short img_y_origin;
    unsigned short img_w;
    unsigned short img_h;
    unsigned char bpp;
    unsigned char img_desc;
};

struct ktga_t {
    ktga_header_t header;
    unsigned char * bitmap;
};

int ktga_load(ktga_t * out_tga, void * buffer, unsigned long long int buffer_length);
void ktga_destroy(ktga_t * tga);

#endif

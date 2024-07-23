#include "ktga.hpp"
#include <cstring>

#define U8(buf, i) *(((unsigned char *) buf) + i)
#define U16(buf, i) *(((unsigned char *) buf) + i) | (*(((unsigned char *) buf) + i + 1) << 8)

int ktga_load(ktga_t * out_tga, void * buffer, unsigned long long int buffer_length) {
    if (buffer_length <= 18 || buffer == nullptr) {
        return 1;
    }

    unsigned char * buf = (unsigned char *) buffer;
    if (U8(buf, 2) != 0x02) {
        return 2;
    }
    out_tga->header.id_len = U8(buf, 0);
    out_tga->header.color_map_type = U8(buf, 1);
    out_tga->header.img_type = U8(buf, 2);
    out_tga->header.color_map_origin = U16(buf, 3);
    out_tga->header.color_map_length = U16(buf, 5);
    out_tga->header.color_map_depth = U8(buf, 7);
    out_tga->header.img_x_origin = U16(buf, 8);
    out_tga->header.img_y_origin = U16(buf, 10);
    out_tga->header.img_w = U16(buf, 12);
    out_tga->header.img_h = U16(buf, 14);
    out_tga->header.bpp = U8(buf, 16);
    out_tga->header.img_desc = U8(buf, 17);
    out_tga->bitmap = new unsigned char[out_tga->header.img_w * out_tga->header.img_h * (out_tga->header.bpp / 8)];
    if (out_tga->bitmap == nullptr) {
        return 3;
    }

    switch (out_tga->header.img_type) {
        case 2:
            memcpy(out_tga->bitmap, &buf[18], out_tga->header.img_w * out_tga->header.img_h * (out_tga->header.bpp / 8));
            break;
    }

    return 0;
}

void ktga_destroy(ktga_t * tga) {
    delete tga->bitmap;
}

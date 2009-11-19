// hidim.h
//
// under the WTFPL. see http://sam.zoy.org/wtfpl/
//
//             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE 
//                     Version 2, December 2004 
// 
//  Copyright (C) 2009 solsTiCe d'Hiver
//                           <solstice.dhiver@gmail.com>
//  Everyone is permitted to copy and distribute verbatim or modified 
//  copies of this license document, and changing it is allowed as long 
//  as the name is changed. 
// 
//             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE 
//    TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION 
// 
//   0. You just DO WHAT THE FUCK YOU WANT TO.
//

#ifndef H_HIDIM
#define H_HIDIM 1

#define VERSION "0.1.3.1"
#define VERSION_STRING "hidim creator v"VERSION" (c) <solstice.dhiver@gmail.com>"

#define CONFIG_BORDER        1
#define CONFIG_COMMENT      (1 << 1)
#define CONFIG_OVERWRITE    (1 << 2)
#define CONFIG_VERBOSE      (1 << 3)
#define CONFIG_MORE_VERBOSE (1 << 4)

#define HIDIM_KEY { \
    104, 105, 100, 105, 109, 32, 105, 115, 32, 116, 111, 114, \
    114, 101, 110, 116, 115, 33 \
}
#define HIDIM_KEY_SIZE 18
#define HIDIM_KEY_STRING "hidim is torrents!"

#define PNG_HEADER_SIZE 8

#define PIXEL_SIZE 3    // should replace almost all occurence of 3

#define BLOCK_SIZE 1024

#define DEFAULT_LINE_LENGTH 30
#define MIN_LINE_LENGTH 8   // 6 pixels for hidim header + 1 (or 2 ?) pixel for line length
#define MAX_LINE_LENGTH 2048    // arbitrary choosen value
#define MIN_WIDTH 10    // used for a warning message. 10 is an arbitrary choosen value

#define BANNER_WIDTH 5
#define BANNER_HEIGHT 19
/* raw pixel data of hidim.png, the hid.im logo/banner */
static png_byte banner[BANNER_HEIGHT * BANNER_WIDTH * 3] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0,
    255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255,
    255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,
    0, 0, 0, 0, 0, 255, 255, 255, 0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255,
    255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

void usage(const char *progname);
bool is_file(const char *filename);
char *strtoupper(const char *s);
char *hexa_sha1sum(unsigned char md[20]);
char *hexa_sha1sum_file(const char *filename);
char *extension(const char*filename);
int digits(unsigned int i);
char *get_pngfile(const char *torrentfilename);
void free_array(png_bytep *array, png_uint_32 length);

int read_png(FILE *fp, png_bytep **row_pointers, png_uint_32 *width,
        png_uint_32 *height);
png_bytep *rotate_image(png_bytep *row_pointers, png_uint_32 width,
        png_uint_32 height);
bool search_key(png_bytep *transpose, png_uint_32 width, png_uint_32 height,
        int *row, int *column);
int read_metadata(png_bytep *transpose, int start_row, int start_column,
        int *line_length, char **filename, char **sha1sum,
        unsigned int *metadata_length, unsigned int *length);
char *create_metadata(unsigned int *length, int line_length,
        const char *torrentfilename);
int aggregate_data(png_bytep *row_pointers, png_uint_32 height, png_uint_32 width,
        int line_length, unsigned int length, const char *metadata,
        const char *torrentfilename);
int write_png(png_bytep *row_pointers, png_uint_32 width, png_uint_32 height,
        const char *filename);
png_bytep *add_banner(png_bytep *row_pointers, png_uint_32 *width,
        png_uint_32 *height);
int encode(const char *torrentfilename, int line_length);
int decode(const char *pngfilename);

#endif /* H_HIDIM */

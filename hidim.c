// this is C99

// hidim.c                                  Version 0.1.3.1      2009-09-14
//
// under the WTFPL. see http://sam.zoy.org/wtfpl/
//
//             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE 
//                     Version 2, December 2004 
// 
//  Copyright (C) 2009 solsTiCe d'Hiver              <solstice.dhiver@gmail.com>
//  Everyone is permitted to copy and distribute verbatim or modified 
//  copies of this license document, and changing it is allowed as long 
//  as the name is changed. 
// 
//             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE 
//    TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION 
// 
//   0. You just DO WHAT THE FUCK YOU WANT TO.
//

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>

#include <png.h>
#include <openssl/sha.h>
#include <sys/stat.h>

#include "hidim.h"

/* global variable for configuration */
static int config = CONFIG_BORDER | CONFIG_COMMENT;

/* is this a readable file ? */
bool is_file(const char *filename) {
    struct stat buf;
    int ret = stat(filename, &buf);
    return !ret && (S_ISREG(buf.st_mode) || S_ISLNK(buf.st_mode));
}

/* turn a string into lowercase */
char *strtolower(const char *s) {
    char *t = strdup(s), *c = t;
    while (*c != '\0') {
        *c = tolower(*c);
        c++;
    }
    return t;
}

/* turn a sha1sum into an hexa string */
char *hexa_sha1sum(unsigned char md[]) {
    char *sha1sum_comp;
    sha1sum_comp = malloc(41);
    if (!sha1sum_comp) abort();
    for (int i=0; i< 20; i++) {
        sprintf(sha1sum_comp + 2*i, "%02x", md[i]);
    }
    return sha1sum_comp;
}

/* return the hexa sha1sum of a file */
char *hexa_sha1sum_file(const char *filename) {
    SHA_CTX c;
    unsigned char md[20], *data;
    FILE *fp;
    size_t len;

    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        return NULL;
    }
    /* read the file by chunk of BLOCK_SIZE size */
    data = malloc(BLOCK_SIZE);
    SHA1_Init(&c);
    while (!feof(fp)) {
        len = fread(data, 1, BLOCK_SIZE, fp);
        SHA1_Update(&c, data, len);
    }
    fclose(fp);
    SHA1_Final(md, &c);
    free(data);
    return hexa_sha1sum(md);
}

void usage(const char *progname) {
    printf("Usage: %s [options] [file1.torrent|file1.png] ...\n"
"Converts a torrent to a hidim or a hidim to a torrent.\n"
"\n"
"Options:\n"
"    -h: print this help\n"
"    -f: force overwrite of existing file\n"
"    -v: be verbose\n"
"    -vv: be more verbose\n"
"    -V: print version information\n"
"when encoding a hidim (these have no effect while decoding)\n"
"    -l NUMBER: line length of the hidim (default: 30)\n"
"    -n: do not add a border and the hidim logo\n"
"    -s: stealth/do not add comment to png\n", progname);
}

/* get the extension of the filename */
char *extension(const char*filename) {
    return strrchr(filename, '.');
}

/* read the png image to fill row_pointers */
int read_png(FILE *fp, png_bytep **row_pointers, png_uint_32 *width, png_uint_32 *height) {
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 color_type;
    unsigned char header[PNG_HEADER_SIZE];

    /* check if it's a png file */
    size_t bytes_read = fread(header, sizeof(unsigned char), PNG_HEADER_SIZE, fp);
    if (ferror(fp) || (bytes_read != PNG_HEADER_SIZE)) {
        return 0;
    }
    if (png_sig_cmp(header, 0, PNG_HEADER_SIZE) != 0) {
        return 0;
    }

    /* initialize libpng stuff */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, PNG_HEADER_SIZE); // because of the check above
    png_read_info(png_ptr, info_ptr);

    *height = png_get_image_height(png_ptr, info_ptr);
    *width = png_get_image_width(png_ptr, info_ptr);
    *row_pointers = png_malloc(png_ptr, *height*sizeof(png_bytep));
    for (int i=0; i<*height; i++) {
        (*row_pointers)[i] = png_malloc(png_ptr, *width*PIXEL_SIZE);
    }
    png_set_rows(png_ptr, info_ptr, *row_pointers);

    color_type = png_get_color_type(png_ptr, info_ptr);
    /* we don't need alpha channel */
    if (color_type & PNG_COLOR_MASK_ALPHA) {
        png_set_strip_alpha(png_ptr);
    }
    png_read_update_info(png_ptr, info_ptr);

    /* and ... finally read the image */
    png_read_image(png_ptr, *row_pointers);
    png_read_end(png_ptr, NULL);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return 1;
}

/* rotate image of 90° clockwise */
png_bytep *rotate_image(png_bytep *row_pointers, png_uint_32 width, png_uint_32 height) {
    png_bytep *transpose = NULL;

    if ((transpose = calloc(width, sizeof(png_bytep))) == NULL) {
        fprintf(stderr, "Error: can't allocate %lu bytes\n", width*sizeof(png_bytep));
        exit(EXIT_FAILURE);
    }

    for (int i=0; i<width; i++) {
        if ((transpose[i] = calloc(height*PIXEL_SIZE, sizeof(png_byte))) == NULL) {
            fprintf(stderr, "Error: can't allocate %lu bytes\n", height*PIXEL_SIZE*sizeof(png_byte));
            exit(EXIT_FAILURE);
        }
    }

    for (int i=0; i<height; i++) {
        for(int j=0; j<width; j++) {
            for (int n=0; n<PIXEL_SIZE; n++) {
                transpose[j][height*3-i*3+n-3] = row_pointers[i][3*j+n];
            }
        }
    }

    return transpose;
}

/* search for the hidim key in the rotated pixel matrix 
 * return true if the key has been found, then update row and column with the
 * value of the beginning of the hidim key
 * return false otherwise */
bool search_key(png_bytep *transpose, png_uint_32 width, png_uint_32 height,
        int *row, int *column) {
    png_byte hidim_key[] = HIDIM_KEY;
    bool found = false;
    int i = 0, j, n;

    while ((i<width) && (!found)) {
        for (j=0; j<height-HIDIM_KEY_SIZE; j++) {
            /* first, look for the first byte */
            if (transpose[i][j*PIXEL_SIZE] == hidim_key[0]) {
                /* then check if it's good */
                n = 1;
                while ((n<HIDIM_KEY_SIZE) && (transpose[i][j*3+n] == hidim_key[n])) {
                    n++;
                }
                found = (n == HIDIM_KEY_SIZE);
                if (found) break;
            }
        }
        if (!found) i++;
    }
    *row = i;
    *column = j;
    return found;
}

/* read the metadata of the hidim:
 * - the length of the line
 * - the filename
 * - the sha1sum of the torrent
 * - metadata length
 * - and the length of the torrent we need to read next */
int read_metadata(png_bytep *transpose, int start_row, int start_column,
        int *line_length, char **filename, char **sha1sum, unsigned int *metadata_length, unsigned int *length) {
    char *start, *end, *buf, *metadata;
    const char *start_hidim;
    int c = 0, s = 0, count = 0, filename_length;

    start_hidim = (const char *)(transpose[start_row]+3*start_column+HIDIM_KEY_SIZE); 
    /* look for line length */
    start = strchr(start_hidim, 'i');
    assert(start == start_hidim);
    end = strchr(start_hidim, 'e');
    if (start == NULL || end == NULL) {
        fprintf(stderr, "Error: wrong bencoded data in metadata of hidim\n");
        return 0;
    }
    buf = calloc(20, sizeof(char));
    strncpy(buf, start+1, end-1-start);
    *line_length = atoi(buf);

    /* look for filename length */
    start = end+1;
    end = strchr(start, ':');
    if (end == NULL) {
        fprintf(stderr, "Error: wrong bencoded data in metadata of hidim\n");
        free(buf);
        return 0;
    }
    memset(buf, 0, sizeof(char)*20);
    strncpy(buf, start, end-start);
    filename_length = atoi(buf);

    *metadata_length = HIDIM_KEY_SIZE + (end - start_hidim + 1) +
        filename_length + 3 + 40 ;
    /* add 20 because we don't know the size of last field length */
    metadata = calloc(*metadata_length + 1 + 20, sizeof(char));

    /* get all the metadata */
    while (count<*metadata_length) {
        metadata[count] = (unsigned char)transpose[start_row+s][3*start_column+c];
        if (c == (*line_length*3 -1) ) {
            c = 0;
            s++;
        } else {
            c++;
        }
        count++;
    }

    *filename = calloc(filename_length + 1, sizeof(char));
    strncpy(*filename, metadata + (end - start_hidim) + 1 + HIDIM_KEY_SIZE, filename_length);

    *sha1sum = calloc(40 + 1, sizeof(char));
    strncpy(*sha1sum, metadata + (end - start_hidim) + 1 + HIDIM_KEY_SIZE + filename_length + 3, 40);

    /* get also the length metadata field */
    do {
        metadata[count] = (unsigned char)transpose[start_row+s][3*start_column+c];
        if (c == (*line_length*3 -1) ) {
            c = 0;
            s++;
        } else {
            c++;
        }
        count++;
    } while (metadata[count-1] != 'e');

    /* get length metadata field */
    start = strrchr(metadata, 'i');
    end = strrchr(metadata, 'e');
    memset(buf, 0, sizeof(char)*20);
    strncpy(buf, start+1, end-start+1);
    *length = atoi(buf);
    free(buf);

    *metadata_length = strlen(metadata);
    free(metadata);

    return 1;
}

void free_array(png_bytep *array, png_uint_32 length) {
    for (int i=0; i<length; i++) {
        free(array[i]);
    }
    free(array);
}

/* decode hidim png file and write the torrent file */
int decode(const char *pngfilename) {
    FILE *fp;
    png_uint_32 height, width;
    png_bytep *row_pointers = NULL, *transpose = NULL;
    char *filename, *basefilename, *sha1sum;
    int start_row, start_column, line_length, c;
    unsigned int length, metadata_length, count, s;

    if ((fp = fopen(pngfilename, "rb")) == NULL) {
        fprintf(stderr, "%s: %s\n", pngfilename, strerror(errno));
        return 0;
    }
    if (config & CONFIG_VERBOSE) {
        printf(":: decoding %s\n", pngfilename);
    }

    if (!read_png(fp, &row_pointers, &width, &height)) {
        fclose(fp);
        fprintf(stderr, "Error: %s is not a png file.\n", pngfilename);
        return 0;
    }
    fclose(fp);

    /* rotate image */
    transpose = rotate_image(row_pointers, width, height);
    free_array(row_pointers, height);
    if (transpose == NULL) { // it can't be. already checked before
        exit(EXIT_FAILURE);
    }

    /* search for hidim key */
    if (!search_key(transpose, width, height, &start_row, &start_column)) {
        fprintf(stderr, "Error: %s is not a hidim or is corrupted.\n", pngfilename);
        free_array(transpose, width);
        return 0;
    } else if (config & CONFIG_MORE_VERBOSE) {
        printf("  hidim key found at (%d, %d)\n", start_row, (int)(height-start_column-1));
    }

    /* extract metadata */
    read_metadata(transpose, start_row, start_column, &line_length, &filename, &sha1sum, &metadata_length, &length);

    /* for security reason, only keep the basename of filename */
    basefilename = basename(filename);
    if (config & CONFIG_MORE_VERBOSE) {
        printf("==== metadata (%d bytes) ====\n", metadata_length);
        printf("  line_length: %d  sha1sum: %s  length: %d\n", line_length, sha1sum, length);
        printf("  filename   : %s\n", filename);
        printf("====\n");
        //printf(":: %d %s %s %d %d\n", line_length, basefilename, sha1sum, metadata_length, length);
    }
    /* get the torrent */
    unsigned char *torrent = calloc(length, sizeof(char));
    if (torrent == NULL) {
        fprintf(stderr, "Error: Can't allocate %d bytes.\n", length*sizeof(char));
        exit(EXIT_FAILURE);
    }

    count = c = s = 0;
    while (count < length+metadata_length) {
        if (count >= metadata_length) {
            torrent[count - metadata_length] = (unsigned char)transpose[start_row+s][3*start_column+c];
        }
        if (c == (line_length*3 -1) ) {
            c = 0;
            s++;
        } else {
            c++;
        }
        count++;
    }
    free_array(transpose, width);

    /* check the sha1sum of the torrent we extracted with sha1sum from metadata */
    unsigned char md[20];
    SHA1(torrent, length, md);
    char *sha1sum_comp = hexa_sha1sum(md);
    if (strcmp(sha1sum_comp, sha1sum) != 0) {
        if (config & CONFIG_MORE_VERBOSE) {
            printf("sha2sum: expected %s, got %s\n", sha1sum, sha1sum_comp);
        }
        fprintf(stderr, "%s: wrong sha1sum for extracted data.\n", filename);
        free(sha1sum_comp);
        free(sha1sum);
        free(torrent);
        free(filename);
        return 0;
    }
    free(sha1sum);
    free(sha1sum_comp);

    /* check if torrent file does not already exist */
    if (is_file(basefilename) && (!(config & CONFIG_OVERWRITE))) {
        fprintf(stderr, "%s already exists. nothing done.\n", basefilename);
        free(torrent);
        free(filename);
        return 0;
    }
    /* write torrent to file */
    FILE *fo = fopen(basefilename, "w");
    fwrite(torrent, sizeof(char), length, fo);
    fclose(fo);
    if (config & CONFIG_VERBOSE) {
        printf("%s has been saved.\n", basefilename);
    }

    free(torrent);
    free(filename);

    return 1;
}

/* return the number of digits of integer n */
int digits(unsigned int n) {
    return snprintf(NULL, 0, "%d", n);
}

/* format the string of the metadata of the hidim
 * length is changed to hold the length of the torrent file */
char *create_metadata(unsigned int *length, int line_length, const char* torrent) {
    struct stat buf;
    int len_filename;
    char *metadata, *sha1sum;

    const char *filename = basename(torrent);
    len_filename = strlen(filename);
    stat(torrent, &buf); // should not failed, checked before
    *length = buf.st_size;

    metadata = calloc(HIDIM_KEY_SIZE + digits(line_length) + digits(*length) +
           digits(len_filename) + len_filename + 40 + 8 + 1, sizeof(char));
    sha1sum = hexa_sha1sum_file(torrent);
    sprintf(metadata, "%si%de%d:%s40:%si%de", HIDIM_KEY_STRING, line_length, 
            len_filename, filename, sha1sum, *length);
    free(sha1sum);

    return metadata;
}

/* fill row_pointers with the metadata and the torrent */
int agregate_data(png_bytep *row_pointers, png_uint_32 height, png_uint_32 width,
       int line_length, unsigned int length, const char *metadata, const char *torrent) {
    FILE *fp;
    int column = 0, row = line_length - 1, m = strlen(metadata)/3;

    /* write metadata */
    for (int i=0; i<m; i++) {
        for (int j=0; j<3; j++) {
            row_pointers[row][column+j] = metadata[i*3+j];
        }
        row--;
        if (row < 0) {
            row = line_length -1;
            column += 3;
        }
    }
    /* writing the only pixel with data from metadata and from torrent */
    m = strlen(metadata) % 3;
    for (int j=0; j<m; j++) {
        row_pointers[row][column+j] = metadata[strlen(metadata)-m+j];
    }

    if ((fp = fopen(torrent, "rb")) == NULL) {
        fprintf(stderr, "%s: %s\n", torrent, strerror(errno));
        return 0;
    }

    for (int j=m; j<3; j++) {
        fread(row_pointers[row]+column+j, 1, sizeof(char), fp);
    }
    row--;
    if (row < 0) {
        row = line_length -1;
        column += 3;
    }

    /* now the data of the torrent */
    while (!feof(fp)) {
        fread(row_pointers[row]+column, 3, sizeof(char), fp);
        row--;
        if (row < 0) {
            row = line_length -1;
            column += 3;
        }
    }
    fclose(fp);

    return 1;
}

/* get the png filename from the torrent name by changing its extension */
char *get_pngfile(const char *torrent) {
    size_t len = extension(torrent) - torrent;
    char *name = malloc(len + 5);
    if (!name) abort();
    strncpy(name, torrent, len);
    strncpy(name + len, ".png", 5);
    return name;
}

/* write row_pointers data to png file */
int write_png(png_bytep *row_pointers, png_uint_32 width, png_uint_32 height, const char *filename) {
    png_structp png_ptr;
    png_infop info_ptr;
    FILE *fp;

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        return 0;
    }
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    /* add some comments */
    if (config & CONFIG_COMMENT) {
        int num_text = 2;
        png_textp text_ptr = calloc(num_text, sizeof(png_text));

        text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
        text_ptr[0].key = "Software";
        text_ptr[0].text = VERSION_STRING;
        text_ptr[0].text_length = strlen(text_ptr[0].text);

        text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
        text_ptr[1].key = "Comment";
        text_ptr[1].text = "created with "VERSION_STRING;
        text_ptr[1].text_length = strlen(text_ptr[1].text);

        png_set_text(png_ptr, info_ptr, text_ptr, num_text);
        free(text_ptr);
    }
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return 1;
}

/* move the raw pixel data to be able to add a black border and the hid.im logo(banner) 
 *
 * Synopsis :
 * Instead of creating a new image and copying the data we already have, we go 
 * the complicated way:
 * move the data to make some room for the logo and border 
 *
 * xxxxxxxxxxxxxxxxxxxx
 * xxxxx--------------x
 * xxxxx--------------x
 * x@@@@--------------x     Legend:
 * x@@@@--------------x         @ logo pixels
 * x@@@@--------------x         - original pixels
 * x@@@@--------------x         x black pixels (border)
 * x@@@@--------------x
 * x@@@@--------------x
 * xxxxxxxxxxxxxxxxxxxx
 *
 * so we have to :
 * (1) move the rows one pixel down to make some room for the first new row 
 * (black border)
 * (2) move the pixels to the right to make room for the first column (black
 * border) and the logo
 */
png_bytep *add_banner(png_bytep *row_pointers, png_uint_32 *width, png_uint_32 *height) {
    png_uint_32 h = *height+2;
    png_uint_32 w = *width;

    /* add a new last row */
    row_pointers = realloc(row_pointers, h*sizeof(png_bytep));
    row_pointers[h-1] = calloc(3*w, sizeof(png_byte));
    /* shift all the rows */
    for (int i=h-2; i> 0; i--) {
        row_pointers[i] = row_pointers[i-1];
    }
    /* add a new first row */
    row_pointers[0] = calloc(3*w, sizeof(png_byte));

    png_uint_32 old_w = w;
    w += BANNER_WIDTH+2;

    /* make some room for the banner */
    for (int i=0; i<h; i++) {
        row_pointers[i] = realloc(row_pointers[i], 3*w*sizeof(png_byte));
        memmove(row_pointers[i]+(BANNER_WIDTH+1)*3, row_pointers[i], 3*old_w);
    }
    /* turn pixels into black */
    for (int i=0; i<h; i++) {
        for (int n=0; n<3; n++) {
            row_pointers[i][n] = 0;     // the first column
            row_pointers[i][(w-1)*3+n] = 0;     // the last column
        }
        /* the empty left space */
        for (int j=3; j<(BANNER_WIDTH+1)*3; j++) {
            row_pointers[i][j] = 0;
        }
    }

    /* add the banner */
    for (int i=0; i< BANNER_HEIGHT; i++) {
        memcpy(row_pointers[i+h-BANNER_HEIGHT-1]+3, banner+i*BANNER_WIDTH*3, BANNER_WIDTH*3);
    }
    *height =h;
    *width = w;
    return row_pointers;
}

/* encode the torrent into a hidim with line_length  and write the png file */
int encode(const char* torrent, int line_length) {
    char *metadata;
    unsigned int length;

    if (config & CONFIG_VERBOSE) {
        printf(":: encoding %s\n", torrent);
    }

    metadata = create_metadata(&length, line_length, torrent);
    if (metadata == NULL) {
        return 0;
    }
    if (config & CONFIG_MORE_VERBOSE) {
        printf("%s\n", metadata);
    }

    unsigned int total_length = strlen(metadata) + length;
    png_uint_32 height = line_length;
    png_uint_32 width = total_length/(line_length*3) + (total_length%(line_length*3) ? 1 : 0);
    if (width < MIN_WIDTH) {
        fprintf(stderr, "Warning: %s: width of hidim(%d) is very small. Choose a smaller line length\n",
               torrent, (unsigned int)width);
    }
    if (config & CONFIG_MORE_VERBOSE) {
        printf("total_length=%d height=%u width=%u\n", total_length, (unsigned int)height, (unsigned int)width);
    }
    /* allocate memory for the image */
    png_bytep *row_pointers = calloc(height, sizeof(png_bytep));
    for (int i=0; i<height; i++) {
        row_pointers[i] = calloc(3*width, sizeof(png_byte));
    }

    /* construct pixel data from hidim data */
    agregate_data(row_pointers, height, width, line_length, length, metadata, torrent);
    free(metadata);

    /* add the hid.im logo */
    if (config & CONFIG_BORDER) {
        row_pointers = add_banner(row_pointers, &width, &height);
    }

    /* make the name of the png file */
    char *filename = get_pngfile(basename(torrent));

    /* check if file already exists */
    if (is_file(filename) && (!(config & CONFIG_OVERWRITE))) {
        fprintf(stderr, "%s already exists. nothing done.\n", filename);
        free(filename);
        free_array(row_pointers, height);
        return 0;
    }

    write_png(row_pointers, width, height, filename);
    if (config & CONFIG_VERBOSE) {
        printf("%s has been saved\n", filename);
    }
    free(filename);

    free_array(row_pointers, height);

    return 1;
}

int main(int argc, char *argv[]) {
    const char *ext;
    int opt, line_length = DEFAULT_LINE_LENGTH;

#ifndef NDEBUG
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    while ((opt = getopt(argc, argv, "fhl:nsv::V")) != -1) {
        switch(opt) {
            case 'f':
                config |= CONFIG_OVERWRITE;
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'l':
                line_length = atoi(optarg);
                if (line_length <= 0 || line_length > MAX_LINE_LENGTH) {
                    fprintf(stderr, "Warning: incorrect line length. using the default %d.\n", DEFAULT_LINE_LENGTH);
                    line_length = DEFAULT_LINE_LENGTH;
                }
                break;
            case 'n':
                config &= ~CONFIG_BORDER;
                break;
            case 's':
                config &= ~CONFIG_COMMENT;
                break;
            case 'v':
                if (optarg == NULL) {
                    config |= CONFIG_VERBOSE;
                } else if (strcmp(optarg, "v") == 0) {
                    config |= CONFIG_VERBOSE | CONFIG_MORE_VERBOSE;
                } else {
                    fprintf(stderr, "Error: bad argument\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'V':
                printf("%s\n", VERSION_STRING);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* line length is at minimum the BANNER_HEIGHT if there is a banner/border */
    if ((config & CONFIG_BORDER) && (line_length < BANNER_HEIGHT)) {
        fprintf(stderr, "Warning: line length should be at least the height of "
                "the hid.im logo i.e. %d\n", BANNER_HEIGHT);
        line_length = BANNER_HEIGHT;
    }
    if ((!(config & CONFIG_BORDER)) && (line_length < MIN_LINE_LENGTH)) {
        fprintf(stderr, "Warning: line length should be at least %d. using %d "
                "as line length\n", MIN_LINE_LENGTH, MIN_LINE_LENGTH);
        line_length = MIN_LINE_LENGTH;
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i=optind; i<argc; i++) {
        if (!is_file(argv[i])) {
            fprintf(stderr, "Error: %s is not a file. skipped\n", argv[i]);
            continue;
        }

        ext = extension(argv[i]);
        if (ext == NULL) {
            fprintf(stderr, "Error: expecting files with .png or .torrent extension. %s skipped\n", argv[i]);
            continue;
        }
        char *ext_tolower = strtolower(ext);
        if (strcmp(ext_tolower, ".png") == 0) {
            decode(argv[i]);
        } else if (strcmp(ext_tolower, ".torrent") == 0) {
            encode(argv[i], line_length);
        } else {
            fprintf(stderr, "Error: files with .png or .torrent extension expected. %s skipped\n", argv[i]);
            free(ext_tolower);
            continue;
        }
        free(ext_tolower);
    }

    return EXIT_SUCCESS;
}

/* vim: set foldmethod=indent: */

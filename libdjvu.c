/*
 * libdjvu.c DjVu Viewer plugin for Hanlin V3 e-Reader.
 * Copyright (c) 2009 Tigran Aivazian
 * License: GPLv2
 * Version: 1.97
 *
 * This program is loosely based on the djvuparser plugin by Jinke.
 *
 * Thanks to the following people for their valuable suggestions:
 *     Vadim Lopatin <vadim.lopatin@coolreader.org>
 *     Leon Bottou <leon@bottou.org>
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <libdjvu/ddjvuapi.h>
#include "libdjvu.h"
#include "debug.h"
#include "keyvalue.h"

#define LIBDJVU_VERSION  "1.97"

#define SCREEN_WIDTH    600
#define SCREEN_HEIGHT   800

/* select ereader's model */
#define HANLIN_V3  0
#define HANLIN_V5  1

/* 
   the latest Hanlin V3 environment switched to 3 bits per pixel
   even though the hardware apparently has only 2 bits. Moreover,
   they now use up the whole byte for each pixel (previously 4 pixels per byte)
   so we need to adjust our buffers and bitmap conversion algorithms accordingly.
 
   This means that if you are building libdjvu for the latest Hanlin V3 environment
   (using arm-linux-gnueabi) you need to select HANLIN_V5 model even though it is a V3.
 */

#if EREADER_MODEL == HANLIN_V5
#define PIXELS_PER_BYTE  1
#define SCREEN_BUFFER_SIZE (SCREEN_WIDTH*SCREEN_HEIGHT+1)
#define WHITE_BLOCK_SIZE (INPUT_BLOCK_WIDTH*INPUT_BLOCK_HEIGHT+1)
#endif

#if EREADER_MODEL == HANLIN_V3
#define PIXELS_PER_BYTE  4
#define SCREEN_BUFFER_SIZE ((SCREEN_WIDTH+3)*SCREEN_HEIGHT/4)
#define WHITE_BLOCK_SIZE ((INPUT_BLOCK_WIDTH+3)*INPUT_BLOCK_HEIGHT/4)
#endif

#if DEBUG
#define PAGE_BACKGROUND 0
#include <errno.h>
#include <sys/times.h>
FILE *logfp;
#define PROFILE_START DPRINTF("%s: ", __FUNCTION__); struct tms tms1, tms2; times(&tms1);
#define PROFILE_STOP  times(&tms2); DPRINTF("%ld/%ld ticks\n", tms2.tms_utime - tms1.tms_utime, tms2.tms_stime - tms1.tms_stime);
#else
#define PAGE_BACKGROUND 0xFF
#define PROFILE_START /* nothing */
#define PROFILE_STOP  /* nothing */
#endif

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

static int page_number, old_page_number, page_width, page_height, numpages;
ddjvu_page_type_t page_type;
static float page_aspect;
static struct timeval tvstart, tvstop;
static unsigned int page_render_time_ms, page_decode_time_ms;
static int landscape, old_landscape, buffer_valid, next_page_bottom, next_page_top;
static float zoom_factor, old_zoom_factor;
static int zoom_factor_inc, old_zoom_factor_inc; /* in percent */
static int horiz_shift_factor, old_horiz_shift_factor, vert_shift_factor, old_vert_shift_factor; /* in percent */
static int *input_buffer, min_input_value, max_input_value, waiting_for_a_key;
int old_window_pos, show_wmark, old_show_wmark, user_djvu_render_mode, multicol, old_multicol;

/* some details of the djvu file being read */
static char *base_file_name, *dir_name;
static struct stat file_stat;

/* for file.djvu the config file is called file.djvu.ini */
static char inifname[512];

/* starting position for input cursor */
#define INPUT_CURSOR_X      80
#define INPUT_CURSOR_Y     766
#define INPUT_BLOCK_WIDTH   50
#define INPUT_BLOCK_HEIGHT  25

#define MINZOOMSTEP       1
#define MAXZOOMSTEP     800 // could be anything but 800% max is reasonable
#define MAXHSHIFT       800 // could be anything but 800% max is reasonable
#define MAXVSHIFT       800 // could be anything but 800% max is reasonable

#if PIXELS_PER_BYTE == 4
/* GREYSCALE 8-bit bitmap */
static unsigned char imagebuf[SCREEN_WIDTH*SCREEN_HEIGHT+1];
#endif

static unsigned char screenbuf[SCREEN_BUFFER_SIZE];
static unsigned char whiteblock[] = {[0 ... WHITE_BLOCK_SIZE] = 0xFF};

/* various handles for interacting with djvulibre */
ddjvu_context_t *djvu_context;
ddjvu_document_t *djvu_document;
ddjvu_page_t *djvu_page, *djvu_page_next;
ddjvu_format_t *djvu_format;
ddjvu_render_mode_t djvu_render_mode;
ddjvu_rect_t prect, rrect, old_prect, old_rrect;

static struct CallbackFunction *v3_callbacks;
#define DJVULOGDIR  "/home/logs"
#define DJVULOGFILE "/home/logs/libdjvulog.txt"

void SetCallbackFunction(struct CallbackFunction *cb)
{
// we open it here because this is the first function called by the viewer.
#if DEBUG
    mkdir(DJVULOGDIR, 0777);
    logfp = fopen(DJVULOGFILE, "w");
    if (!logfp)
        fprintf(stderr, "%s: Can't open logfile (%s)\n", __FUNCTION__, strerror(errno));
    else
        setbuf(logfp, NULL);
#endif
    DPRINTF("%s\n", __FUNCTION__);
    v3_callbacks = cb;
}

// set the optimal rendering mode based on page_type
static inline void set_djvu_render_mode(void)
{
    if (page_type == DDJVU_PAGETYPE_BITONAL)
        djvu_render_mode = DDJVU_RENDER_MASKONLY;
    else
        djvu_render_mode = DDJVU_RENDER_COLOR;
}

// returns 1 on success, 0 on error
static inline int page_decoded_ok(void)
{
    gettimeofday(&tvstart, NULL);
    while (!ddjvu_page_decoding_done(djvu_page)) {
        ddjvu_message_wait(djvu_context);
        while (ddjvu_message_peek(djvu_context))
            ddjvu_message_pop(djvu_context);
    }
    gettimeofday(&tvstop, NULL);
    page_decode_time_ms = 1000*(tvstop.tv_sec - tvstart.tv_sec) + (tvstop.tv_usec - tvstart.tv_usec)/1000;
    if (ddjvu_page_decoding_error(djvu_page))
       return 0;
    return 1;
}

static inline void wait_for_ddjvu_message(ddjvu_context_t *ctx, ddjvu_message_tag_t mid)
{
    ddjvu_message_wait(ctx);
    const ddjvu_message_t *msg;
    while ((msg = ddjvu_message_peek(ctx)) && msg && (msg->m_any.tag != mid))
        ddjvu_message_pop(ctx);
}

void vSetCurPage(int p)
{
    DPRINTF("%s(%d)\n", __FUNCTION__, p);
    if (p < 0)
         p = 0;
    else if (p >= numpages)
         p = numpages - 1;
    page_number = p;
}

int GetPageNum(void)
{
    DPRINTF("%s() -> %d\n", __FUNCTION__, numpages);
    return numpages;
}

int GetPageIndex(void)
{
    DPRINTF("%s() -> %d\n", __FUNCTION__, page_number);
    return page_number;
}

void vGetTotalPage(int *iTotalPage)
{
    DPRINTF("%s() -> %d\n", __FUNCTION__, numpages);
    *iTotalPage = numpages;
}

int iGetDocPageWidth(void)
{
    DPRINTF("%s() -> %d\n", __FUNCTION__, SCREEN_WIDTH);
    return SCREEN_WIDTH;
}

int iGetDocPageHeight(void)
{
    DPRINTF("%s() -> %d\n", __FUNCTION__, SCREEN_HEIGHT);
    return SCREEN_HEIGHT;
}

void GetPageDimension(int *width, int *height)
{
    DPRINTF("%s() -> %dx%d\n", __FUNCTION__, SCREEN_WIDTH, SCREEN_HEIGHT);
    *width = SCREEN_WIDTH;
    *height = SCREEN_HEIGHT;
    return;
}

void SetPageDimension(int width, int height)
{
    DPRINTF("%s(%d,%d)\n", __FUNCTION__, width, height);
    return;
}

int bGetRotate(void)
{
    DPRINTF("%s() -> 0\n", __FUNCTION__);
    return 0;
}

void vSetRotate(int rot)
{
    DPRINTF("%s(%d)\n", __FUNCTION__, rot);
    return;
}

double dGetResizePro(void)
{
    DPRINTF("%s() -> 1.0\n", __FUNCTION__);
    return 1.0;
}

void vSetResizePro(double dSetPro)
{
    DPRINTF("%s(%f)\n", __FUNCTION__, dSetPro);
    return;
}

int OnStatusInfoChange(status_info_t *statusInfo, myrect *rectToUpdate)
{
    DPRINTF("%s() -> 0\n", __FUNCTION__);
    return 0;
}

int IsStandardStatusBarVisible(void)
{
    DPRINTF("%s() -> 0\n", __FUNCTION__);
    return 0;
}

void bGetUserData(void **vUserData, int *iUserDataLength)
{
    DPRINTF("%s()\n", __FUNCTION__);
    return;
}

void vSetUserData(void *vUserData, int iUserDataLength)
{
    DPRINTF("%s()\n", __FUNCTION__);
    return;
}

unsigned short usGetLeftBarFlag(void)
{
    DPRINTF("%s() -> 4\n", __FUNCTION__);
    return 4;
}

void vEndInit(int iEndStyle)
{
    DPRINTF("%s(%d)\n", __FUNCTION__, iEndStyle);
    return;
}

int Origin(void)
{
    DPRINTF("%s() -> 1\n", __FUNCTION__);
    return 1;
}

int Bigger(void)
{
    DPRINTF("%s() -> 0\n", __FUNCTION__);
    return 0;
}

int Smaller(void)
{
    DPRINTF("%s() -> 0\n", __FUNCTION__);
    return 0;
}

int Rotate(void)
{
    DPRINTF("%s() -> 1\n", __FUNCTION__);
    return 1;
}

int Fit(void)
{
    DPRINTF("%s() -> 1\n", __FUNCTION__);
    return 1;
}

int GotoPage(int n)
{
    int distance;
    DPRINTF("%s(%d)\n", __FUNCTION__, n);
    if (n < 0)
        n = 0;
    else if (n >= numpages)
        n = numpages - 1;
    ddjvu_page_release(djvu_page);
    djvu_page = ddjvu_page_create_by_pageno(djvu_document, n);
    if (!djvu_page) {
        DPRINTF("%s: ddjvu_page_create_by_pageno() page=%d failed\n", __FUNCTION__, n);
        return 0;
    }
    old_window_pos = -1;
    if (!page_decoded_ok()) {
        DPRINTF("%s: decoding failed on page %d\n", __FUNCTION__, n);
        return 1;
    } else {
        ddjvu_page_release(djvu_page_next);
        djvu_page_next = ddjvu_page_create_by_pageno(djvu_document, n + 1);
    }
    page_width = ddjvu_page_get_width(djvu_page);
    page_height = ddjvu_page_get_height(djvu_page);
    page_aspect = (float)page_height/(float)page_width;
    page_type = ddjvu_page_get_type(djvu_page);
    if (!user_djvu_render_mode)
        set_djvu_render_mode();

    // set up page and rendering rectangles
    if (landscape) {
        prect.h = (unsigned int)((float)SCREEN_HEIGHT * zoom_factor);
        prect.w = (unsigned int)((float)prect.h * page_aspect);
        rrect.w = min(prect.w, SCREEN_WIDTH);
        rrect.h = min(prect.h, SCREEN_HEIGHT);
        distance = (int)(prect.w - rrect.w);
        if (next_page_top || rrect.x > distance) {
            rrect.x = distance;
            if (multicol) rrect.y = 0;
        }
        if (next_page_bottom)
            rrect.x = 0;
        distance = (int)(prect.h - rrect.h);
        if (rrect.y > distance || (multicol && next_page_bottom))
            rrect.y = distance;
    } else {
        prect.w = (unsigned int)((float)SCREEN_WIDTH * zoom_factor);
        prect.h = (unsigned int)((float)prect.w * page_aspect);
        rrect.w = min(prect.w, SCREEN_WIDTH);
        rrect.h = min(prect.h, SCREEN_HEIGHT);
        distance = (int)(prect.w - rrect.w);
        if (rrect.x > distance || (multicol && next_page_bottom))
            rrect.x = distance;
        distance = (int)(prect.h - rrect.h);
        if (next_page_bottom || rrect.y > distance)
            rrect.y = distance;
        if (next_page_top) {
            rrect.y = 0;
            if (multicol) rrect.x = 0;
        }
    }

    page_number = n;
    buffer_valid = 0;
    return 1;
}

static inline int goto_prev_page(void)
{
    DPRINTF("%s()\n", __FUNCTION__);
    if (page_number == 0) {
        next_page_top = next_page_bottom = 0;
        return 0;
    }
    return GotoPage(page_number - 1);
}

static inline int goto_next_page(void)
{
    DPRINTF("%s()\n", __FUNCTION__);
    if (page_number == numpages - 1) {
        next_page_top = next_page_bottom = 0;
        return 0;
    }
    return GotoPage(page_number + 1);
}

/* returns non-zero if "filename" is NOT a DjVu file, 0 otherwise */
static inline int not_valid_djvu_file(char *filename)
{
    char buf[9];
    int retval = 1;
    int fd = open(filename, O_RDONLY);

    if (read(fd, buf, 8) == -1)
       goto out;

    if (memcmp(buf, "AT&TFORM", 8))
       goto out;

    retval = 0;
    if (fstat(fd, &file_stat) == -1)
       goto out;

out:
    (void)close(fd);
    return retval;
}

static inline void set_defaults(void)
{
    old_zoom_factor = zoom_factor = 1.0;
    old_zoom_factor_inc = zoom_factor_inc = 10;
    old_horiz_shift_factor = horiz_shift_factor = 95;
    old_vert_shift_factor = vert_shift_factor = 95;
    old_rrect.x = old_rrect.y = rrect.x = rrect.y = 0;
    multicol = old_multicol = show_wmark = old_show_wmark = landscape = old_landscape = page_type = page_width = page_height = page_number = user_djvu_render_mode = 0;
    page_aspect = 0.0;
    djvu_render_mode = DDJVU_RENDER_MASKONLY;
    old_window_pos = -1; // no need to show any marks as initially there is no "previous window".
    old_page_number = -1;
}

int InitDoc(char *filename)
{
    DPRINTF("%s(%s)\n", __FUNCTION__, filename);

    if (not_valid_djvu_file(filename)) {
        if (strstr(filename, ".exe.djvu")) {
           DPRINTF("%s -> fork()\n", __FUNCTION__);
           pid_t pid = fork();
           if (!pid) {
              int err;
              DPRINTF("%s -> exec(\"%s\")\n", __FUNCTION__, filename);
              err = execve(filename, NULL, NULL);
              if (err == -1)
                  DPRINTF("%s -> exec failed, errno = %d (%s)\n", __FUNCTION__, errno, strerror(errno));
              exit(0);
           } else {
              DPRINTF("%s waiting for the child...\n", __FUNCTION__);
              waitpid(pid, NULL, 0);
              DPRINTF("%s child terminated\n", __FUNCTION__);
              exit(0);
           }
        }
        DPRINTF("%s: \"%s\" not a valid DjVu file\n", __FUNCTION__, filename);
        return 0;
    }
    djvu_context = ddjvu_context_create("libdjvu");
    if (!djvu_context) {
        DPRINTF("%s: ddjvu_context_create() failed\n", __FUNCTION__);
        return 0;
    }
    djvu_document = ddjvu_document_create_by_filename(djvu_context, filename, 1);
    if (!djvu_document) {
        DPRINTF("%s: ddjvu_document_create_by_filename() failed\n", __FUNCTION__);
        return 0;
    }
    base_file_name = basename(strdup(filename));
    dir_name = dirname(strdup(filename));
    djvu_format = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, NULL);
    if (!djvu_format) {
        DPRINTF("%s: ddjvu_format_create() failed\n", __FUNCTION__);
        return 0;
    }
    ddjvu_format_set_row_order(djvu_format, 1);
    ddjvu_format_set_y_direction(djvu_format, 1);
    ddjvu_format_set_ditherbits(djvu_format, 2);
    set_defaults();
    wait_for_ddjvu_message(djvu_context, DDJVU_DOCINFO);
    numpages = ddjvu_document_get_pagenum(djvu_document);
    return 1;
}

#if PIXELS_PER_BYTE == 4

// for masking two bits in screenbuf to OR the data bits.
static unsigned char notmask[4] = {0x3f, 0xcf, 0xf3, 0xfc};

// mapping a greyscale 8-bit pixel "b8b7b6b5b4b3b2b1" to the 2-bit "b8b7"
// and then packing these two bits for each pixel into screenbuf,
// starting from the top.
static inline void grey8to2(void)
{
    int x, y;
    unsigned char tmp;
    unsigned char *dst = screenbuf;

    for (y = 0; y < rrect.h; y++) {
        unsigned char *d = dst;
        int dx = 0;
        int step_y = y * SCREEN_WIDTH;
        for (x=0; x < rrect.w; x++) {
            tmp = imagebuf[x + step_y] >> 6;
            *d &= notmask[dx&3];
            if (!show_wmark || ((landscape ? x : y) != old_window_pos))
                *d |= tmp << ((3 - (dx&3))<<1);
            if ((++dx & 3) == 0) d++;
        }
        dst += (SCREEN_WIDTH + 3)/4;
    }
}
#endif

#if PIXELS_PER_BYTE == 1
static inline void show_window_mark(void)
{
    int x, y;
    unsigned char *dst = screenbuf;

    for (y = 0; y < rrect.h; y++) {
        unsigned char *d = dst;
        for (x=0; x < rrect.w; x++) {
            if ((landscape ? x : y) == old_window_pos) *d = 0;
            d++;
        }
        dst += SCREEN_WIDTH;
    }

}
#endif

#if 0
int signal_level = 255;

static inline int do_trim_margins(void)
{
    DPRINTF("%s\n", __FUNCTION__);
    int x, y, x1, x2, y1, y2, xstart, xend, ystart, yend, width, height;
    unsigned int sum;

    xstart = 0;
    xend = rrect.w/4;
    ystart = 0;
    yend = rrect.h;
    height = rrect.h;
    DPRINTF("%s: testing the X range: %d -> %d\n", __FUNCTION__, xstart, xend);
    for (x = xstart; x < xend; x++) {
        for (sum = 0, y = ystart; y < yend; y++)
            sum += imagebuf[x + y*SCREEN_WIDTH];
        if (sum / height < signal_level) break;
    }
    x1 = max(x, xstart);

    xstart = rrect.w;
    xend = 3*rrect.w/4;
    DPRINTF("%s: testing the X range: %d <- %d\n", __FUNCTION__, xstart, xend);
    for (x = xstart; x > xend; x--) {
        for (sum = 0, y = ystart; y < yend; y++)
            sum += imagebuf[x + y*SCREEN_WIDTH];
        if (sum / height < signal_level) break;
    }
    x2 = min(x, xstart);

    xstart = 0;
    xend = rrect.w;
    ystart = 0;
    yend = rrect.h/4;
    width = rrect.w;
    DPRINTF("%s: testing the Y range: %d -> %d\n", __FUNCTION__, ystart, yend);
    for (y = ystart; y < yend; y++) {
        for (sum = 0, x = xstart; x < xend; x++)
            sum += imagebuf[x + y*SCREEN_WIDTH];
        if (sum / width < signal_level) break;
    }
    y1 = max(y, ystart);
 
    ystart = rrect.h;
    yend = 3*rrect.h/4;
    DPRINTF("%s: testing the Y range: %d <- %d\n", __FUNCTION__, ystart, yend);
    for (y = ystart; y > yend; y--) {
        for (sum = 0, x = xstart; x < xend; x++)
            sum += imagebuf[x + y*SCREEN_WIDTH];
        if (sum / width < signal_level) break;
    }
    y2 = min(y, ystart);

    DPRINTF("%s: x1=%d y1=%d x2=%d y2=%d\n", __FUNCTION__, x1, y1, x2, y2);
    zoom_factor = (float)SCREEN_WIDTH / (float)(x2 - x1);
    rrect.x = x1;
    rrect.y = y1;
    set_page_and_render_rects();
    return 1;
}
#endif

// render a portion of DjVu page if necessary
void GetPageData(void **data)
{
    *data = screenbuf;
    if (buffer_valid) {
        DPRINTF("%s: satisfied from the cache\n", __FUNCTION__);
        return;
    }
    ddjvu_page_set_rotation(djvu_page, landscape ? DDJVU_ROTATE_270 : DDJVU_ROTATE_0);
    gettimeofday(&tvstart, NULL);

#if PIXELS_PER_BYTE == 4
    ddjvu_page_render(djvu_page, djvu_render_mode, &prect, &rrect, djvu_format, SCREEN_WIDTH, (char *)imagebuf);
    memset(screenbuf, PAGE_BACKGROUND, SCREEN_BUFFER_SIZE);
#endif
#if PIXELS_PER_BYTE == 1
    ddjvu_page_render(djvu_page, djvu_render_mode, &prect, &rrect, djvu_format, SCREEN_WIDTH, (char *)screenbuf);
#endif

    buffer_valid = 1;
    while (ddjvu_message_peek(djvu_context)) ddjvu_message_pop(djvu_context);
    gettimeofday(&tvstop, NULL);
    page_render_time_ms = 1000*(tvstop.tv_sec - tvstart.tv_sec) + (tvstop.tv_usec - tvstart.tv_usec)/1000;

#if PIXELS_PER_BYTE == 4
    grey8to2();
#endif
#if PIXELS_PER_BYTE == 1
    if (show_wmark) show_window_mark();
#endif
}

// closing the document, release all the resources.
void vEndDoc(void)
{
    FILE *fp;
    DPRINTF("%s\n", __FUNCTION__);
    if ((fp = fopen(inifname, "w"))) {
        fprintf(fp, "zoom_factor=%f\nzoom_factor_inc=%d\n"
                       "horiz_shift_factor=%d\nvert_shift_factor=%d\n"
                       "landscape=%d\ndjvu_render_mode=%d\nuser_djvu_render_mode=%d\n"
                       "show_wmark=%d\n"
                       "multicol=%d\n"
                       "rrect.x=%d\nrrect.y=%d\n"
                       "page_number=%d",
                        zoom_factor, zoom_factor_inc,
                        horiz_shift_factor, vert_shift_factor,
                        landscape, djvu_render_mode, user_djvu_render_mode,
                        show_wmark,
                        multicol,
                        rrect.x, rrect.y,
                        page_number);
        (void)fclose(fp);
    }
    ddjvu_page_release(djvu_page);
    ddjvu_page_release(djvu_page_next);
    ddjvu_document_release(djvu_document);
    ddjvu_format_release(djvu_format);
    ddjvu_context_release(djvu_context);
#if DEBUG
    (void)fclose(logfp);
#endif
}

static inline void set_page_and_render_rects(void)
{
    int distance;
    if (landscape) {
        prect.h = (unsigned int)((float)SCREEN_HEIGHT * zoom_factor);
        prect.w = (unsigned int)((float)prect.h * page_aspect);
    } else {
        prect.w = (unsigned int)((float)SCREEN_WIDTH * zoom_factor);
        prect.h = (unsigned int)((float)prect.w * page_aspect);
    }
    rrect.w = min(prect.w, SCREEN_WIDTH);
    rrect.h = min(prect.h, SCREEN_HEIGHT);
    distance = (int)(prect.w - rrect.w);
    if (rrect.x > distance)
        rrect.x = distance;
    distance = (int)(prect.h - rrect.h);
    if (rrect.y > distance)
        rrect.y = distance;
    buffer_valid = 0;
    old_window_pos = -1;
}

int iInitDocF(char *filename, int pageno, int flag)
{
    char buf[129];
    FILE *fp;

    DPRINTF("%s(%s,%d,%d)\n", __FUNCTION__, filename, pageno, flag);
    djvu_page = ddjvu_page_create_by_pageno(djvu_document, pageno);
    if (!djvu_page) {
        DPRINTF("%s: ddjvu_page_create_by_pageno() file=%s page=%d failed\n", __FUNCTION__, filename, pageno);
        return 0;
    }
    sprintf(inifname, "%s.ini", filename);
    if (!(fp = fopen(inifname, "r"))) goto out;
    while (fgets(buf, 128, fp)) {
        if (!strncmp(buf, "zoom_factor=", 12))
            zoom_factor = strtof(buf + 12, NULL);
        else if (!strncmp(buf, "zoom_factor_inc=", 16))
            zoom_factor_inc = atoi(buf + 16);
        else if (!strncmp(buf, "horiz_shift_factor=", 19))
            horiz_shift_factor = atoi(buf + 19);
        else if (!strncmp(buf, "vert_shift_factor=", 18))
            vert_shift_factor = atoi(buf + 18);
        else if (!strncmp(buf, "landscape=", 10))
            landscape = atoi(buf + 10);
        else if (!strncmp(buf, "djvu_render_mode=", 17))
            djvu_render_mode = atoi(buf + 17);
        else if (!strncmp(buf, "user_djvu_render_mode=", 22))
            user_djvu_render_mode = atoi(buf + 22);
        else if (!strncmp(buf, "show_wmark=", 11))
            show_wmark = atoi(buf + 11);
        else if (!strncmp(buf, "multicol=", 9))
            multicol = atoi(buf + 9);
        else if (!strncmp(buf, "rrect.x=", 8))
            rrect.x = atoi(buf + 8);
        else if (!strncmp(buf, "rrect.y=", 8))
            rrect.y = atoi(buf + 8);
        else if (!strncmp(buf, "page_number=", 12))
            page_number = atoi(buf + 12);
    }
    (void)fclose(fp);
    if (page_number != pageno) set_defaults(); // invalidate the data from .ini file
out:
    if (!page_decoded_ok()) {
        DPRINTF("%s: decoding of \"%s\" failed on page %d\n", __FUNCTION__, filename, pageno);
        return 0;
    } else
        djvu_page_next = ddjvu_page_create_by_pageno(djvu_document, pageno + 1);
    page_width = ddjvu_page_get_width(djvu_page);
    page_height = ddjvu_page_get_height(djvu_page);
    page_aspect = (float)page_height/(float)page_width;
    page_type = ddjvu_page_get_type(djvu_page);
    if (!user_djvu_render_mode)
        set_djvu_render_mode();
    set_page_and_render_rects();
    page_number = pageno;
    return 0;
}

/* returns 1 if move was successful, otherwise 0 */
static inline int goto_next_column(void)
{
    if (move_window_right() == 1) {
        multicol = 0;
        while (move_window_up()) ;
        multicol = 1;
        old_window_pos = -1;
        return 1;
    }
    return 0;
}

/* returns 1 if move was successful, otherwise 0 */
static inline int goto_prev_column(void)
{
    if (move_window_left() == 1) {
        multicol = 0;
        while (move_window_down()) ;
        multicol = 1;
        old_window_pos = -1;
        return 1;
    }
    return 0;
}

/* returns 0 if can't move, 1 on success */
static inline int move_window_down(void)
{
    int delta, distance;
    DPRINTF("%s()\n", __FUNCTION__);
    next_page_top = next_page_bottom = 0;
    if (landscape) {
        if (rrect.x == 0) {
            DPRINTF("%s: hit the bottom prect.w=%d, rrect.w=%d, rrect.x=%d\n", __FUNCTION__, prect.w, rrect.w, rrect.x);
            if (multicol && goto_next_column()) goto out;
            else {
                next_page_top = 1; // for GotoPage to show the top part
                return 0;
            }
        }
        delta = rrect.w*vert_shift_factor/100;
        old_window_pos = rrect.x;
        if (rrect.x < delta)
            rrect.x = 0;
        else
            rrect.x -= delta;
        old_window_pos -= rrect.x;
    } else {
        distance = (int)(prect.h - rrect.h);
        if (rrect.y == distance) {
            DPRINTF("%s: hit the bottom prect.h=%d, rrect.h=%d, rrect.y=%d\n", __FUNCTION__, prect.h, rrect.h, rrect.y);
            if (multicol && goto_next_column()) goto out;
            else {
                next_page_top = 1; // for GotoPage to show the top part
                return 0;
            }
        }
        delta = rrect.h*vert_shift_factor/100;
        old_window_pos = (int)rrect.h + rrect.y;
        if (rrect.y + delta > distance)
            rrect.y = distance;
        else
            rrect.y += delta;
        old_window_pos -= rrect.y;
    }
out:
    buffer_valid = 0;
    return 1;
}

/* returns 0 if can't move, 1 on success */
static inline int move_window_up(void)
{
    int delta, distance;
    DPRINTF("%s()\n", __FUNCTION__);
    next_page_top = next_page_bottom = 0;
    if (landscape) {
        distance = (int)(prect.w - rrect.w);
        if (rrect.x == distance) {
            DPRINTF("%s: hit the top prect.w=%d, rrect.w=%d, rrect.x=%d\n", __FUNCTION__, prect.w, rrect.w, rrect.x);
            if (multicol && goto_prev_column()) goto out;
            else {
                next_page_bottom = 1; // for GotoPage to show the bottom part
                return 0;
            }
        }
        delta = rrect.w*vert_shift_factor/100;
        old_window_pos = (int)rrect.w + rrect.x;
        if (rrect.x + delta > distance)
            rrect.x = distance;
        else
            rrect.x += delta;
        old_window_pos -= rrect.x;
    } else {
        if (rrect.y == 0) {
            DPRINTF("%s: hit the top prect.h=%d, rrect.h=%d, rrect.y=%d\n", __FUNCTION__, prect.h, rrect.h, rrect.y);
            if (multicol && goto_prev_column()) goto out;
            else {
                next_page_bottom = 1; // for GotoPage to show the bottom part
                return 0;
            }
        }
        delta = rrect.h*vert_shift_factor/100;
        old_window_pos = rrect.y;
        if (rrect.y < delta)
            rrect.y = 0;
        else
            rrect.y -= delta;
        old_window_pos -= rrect.y;
    }
out:
    buffer_valid = 0;
    return 1;
}

static inline int move_window_right(void)
{
    int delta, distance;
    if (landscape) {
        distance = (int)(prect.h - rrect.h);
        if (rrect.y == distance) return 2;
        delta = rrect.h*horiz_shift_factor/100;
        if (rrect.y + delta > distance)
            rrect.y = distance;
        else
            rrect.y += delta;
    } else {
        distance = (int)(prect.w - rrect.w);
        if (rrect.x == distance) return 2;
        delta = rrect.w*horiz_shift_factor/100;
        if (rrect.x + delta > distance)
            rrect.x = distance;
        else
            rrect.x += delta;
    }
    buffer_valid = 0;
    return 1;
}

static inline int move_window_left(void)
{
    int delta;
    if (landscape) {
        if (rrect.y == 0) return 2;
        delta = rrect.h*horiz_shift_factor/100;
        if (rrect.y < delta)
            rrect.y = 0;
        else
            rrect.y -= delta;
    } else {
        if (rrect.x == 0) return 2;
        delta = rrect.w*horiz_shift_factor/100;
        if (rrect.x < delta)
            rrect.x = 0;
        else
            rrect.x -= delta;
    }
    buffer_valid = 0;
    return 1;
}

int Next(void)
{
    int retval;
    DPRINTF("%s()\n", __FUNCTION__);
    retval = landscape ? move_window_up() : move_window_down();
    if (retval)
        goto out;
    retval = landscape ? goto_prev_page() : goto_next_page();
out:
    return retval;
}

int Prev(void)
{
    DPRINTF("%s()\n", __FUNCTION__);
    int retval = landscape ? move_window_down() : move_window_up();
    if (retval)
        goto out;
    retval = landscape ? goto_next_page() : goto_prev_page();
out:
    return retval;
}

static inline void add_text(char *buf, char *text)
{
    DPRINTF("%s(\"%s\")\n", __FUNCTION__, text ? : "NULL");
    if (text) strcat(buf, text);
    v3_callbacks->BlitBitmap(INPUT_CURSOR_X, INPUT_CURSOR_Y, INPUT_BLOCK_WIDTH, INPUT_BLOCK_HEIGHT, 0, 0, INPUT_BLOCK_WIDTH, INPUT_BLOCK_HEIGHT, whiteblock);
    v3_callbacks->TextOut(INPUT_CURSOR_X+5, INPUT_CURSOR_Y+24, buf, strlen(buf), TF_ASCII);
    v3_callbacks->PartialPrint();
}

static inline int process_input_key(int key)
{
    static char buf[8];
    static int nchars = 0;
    int tmp, retval = 2;

    DPRINTF("%s(%d)\n", __FUNCTION__, key);

    if (waiting_for_a_key) {
        waiting_for_a_key = 0;
        v3_callbacks->EndDialog();
        return 1;
    }

    switch (key) {
       case KEY_0:
           if (!nchars)
               break; // [decimal] number cannot start with 0
           else if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "0");
           break;
       case KEY_1:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "1");
           break;
       case KEY_2:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "2");
           break;
       case KEY_3:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "3");
           break;
       case KEY_4:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "4");
           break;
       case KEY_5:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "5");
           break;
       case KEY_6:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "6");
           break;
       case KEY_7:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "7");
           break;
       case KEY_8:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "8");
           break;
       case KEY_9:
           if (++nchars > 3) {
               nchars--;
               break;
           }
           add_text(buf, "9");
           break;
       case KEY_CANCEL:
           if (!nchars) {
               memset(buf, 0, 8);
               nchars = 0;
               v3_callbacks->EndDialog();
               retval = 1;
           } else {
               buf[strlen(buf)-1] = '\0';
               nchars--;
               add_text(buf, NULL);
           }
           break;
       case KEY_OK:
           if (strlen(buf)) {
               tmp = atoi(buf);
               if (tmp < min_input_value)
                   tmp = min_input_value;
               else if (tmp > max_input_value)
                   tmp = max_input_value;
               *input_buffer = tmp;
           }
           memset(buf, 0, 8);
           nchars = 0;
           v3_callbacks->EndDialog();
           retval = 1;
           break;
    }
    return retval;
}

static inline void save_window(void)
{
    DPRINTF("%s\n", __FUNCTION__);
    old_prect = prect;
    old_rrect = rrect;
    old_zoom_factor = zoom_factor;
    old_zoom_factor_inc = zoom_factor_inc;
    old_horiz_shift_factor = horiz_shift_factor;
    old_vert_shift_factor = vert_shift_factor;
    old_landscape = landscape;
    old_multicol = multicol;
    old_show_wmark = show_wmark;
    old_page_number = page_number;
}

static inline void restore_window(void)
{
    DPRINTF("%s\n", __FUNCTION__);
    if (old_page_number == -1)
        return;
    prect = old_prect;
    rrect = old_rrect;
    zoom_factor = old_zoom_factor;
    horiz_shift_factor = old_horiz_shift_factor;
    vert_shift_factor = old_vert_shift_factor;
    landscape = old_landscape;
    show_wmark = old_show_wmark;
    multicol = old_multicol;
    buffer_valid = 0;
    if (page_number != old_page_number) {
        next_page_top = next_page_bottom = 0;
        GotoPage(old_page_number);
    }
}

int OnKeyPressed(int key, int state)
{
    int retval = 0;

    DPRINTF("%s(%d,%d)\n", __FUNCTION__, key, state);

    if (state == CUSTOMIZESTATE)
        return process_input_key(key);

    if (state != NORMALSTATE)
        return retval;

    switch (key) {
        case LONG_KEY_6:
            if (multicol) {
               if (goto_prev_column()) {
                   retval = 1;
                   break;
               } else {
                   next_page_top = 0;
                   next_page_bottom = 1;
               }
            }
            retval = GotoPage(page_number - 1);
            break;

        case KEY_6:
            if (multicol) {
               if (goto_next_column()) {
                   retval = 1;
                   break;
               } else {
                   next_page_top = 1;
                   next_page_bottom = 0;
               }
            }
            retval = GotoPage(page_number + 1);
            break;

        case LONG_KEY_NEXT:
            retval = landscape ? GotoPage(page_number - 10) : GotoPage(page_number + 10);
            break;

        case LONG_KEY_PREV:
            retval = landscape ? GotoPage(page_number + 10) : GotoPage(page_number - 10);
            break;

        case KEY_5:
            save_window();
            retval = 1;
            break;

        case LONG_KEY_5:
            restore_window();
            retval = 1;
            break;

        case LONG_KEY_OK:
            landscape = 1 - landscape;
        case KEY_EXPANSION:
            zoom_factor = 1.0;
            rrect.x = landscape ? (unsigned int)((float)rrect.h * page_aspect) - rrect.w : 0;
            if (rrect.x < 0) rrect.x = 0;
            rrect.y = 0;
            set_page_and_render_rects();
            retval = 1;
            break;

        case LONG_SHORTCUT_KEY_VOLUME_UP:
            zoom_factor += 2.0*(float)zoom_factor_inc/100;
        case KEY_SHORTCUT_VOLUME_UP:
            zoom_factor += (float)zoom_factor_inc/100;
            set_page_and_render_rects();
            retval = 1;
            break;

        case LONG_SHORTCUT_KEY_VOLUME_DOWN:
            zoom_factor -= 2.0*(float)zoom_factor_inc/100;
        case KEY_SHORTCUT_VOLUME_DOWN:
            zoom_factor -= (float)zoom_factor_inc/100;
            if (zoom_factor < 0.02) zoom_factor = 0.02;
            set_page_and_render_rects();
            retval = 1;
            break;

        case LONG_KEY_EXPANSION:
            djvu_render_mode ++;
            djvu_render_mode %= 7;
            if (djvu_render_mode == 6) {
                user_djvu_render_mode = 0;
                set_djvu_render_mode();
            } else
                user_djvu_render_mode = 1;
            buffer_valid = 0;
            retval = 1;
            break;

        case LONG_KEY_DOWN:
            landscape ? move_window_left() : move_window_right();
            landscape ? move_window_left() : move_window_right();
        case KEY_DOWN:
            retval = landscape ? move_window_left() : move_window_right();
            break;

        case LONG_KEY_UP:
            landscape ? move_window_right() : move_window_left();
            landscape ? move_window_right() : move_window_left();
        case KEY_UP:
            retval = landscape ? move_window_right() : move_window_left();
            break;

        default:
            break;
    }

    return retval;
}

#define DJVU_MENU_ABOUT             2000
#define DJVU_MENU_ZOOMFACTOR_ENTER  2001
#define DJVU_MENU_HSHIFT_ENTER      2002
#define DJVU_MENU_VSHIFT_ENTER      2003
#define DJVU_MENU_SHOW_WMARK        2004
#define DJVU_MENU_MULTICOL          2005
#define DJVU_MENU_HELP              2006

#if EREADER_MODEL == HANLIN_V3
#define VIEWER_MENU_GOTOFIRSTPAGE 118
#define VIEWER_MENU_GOTOENDPAGE   119
#define VIEWER_MENU_GOTOINDEX     121
#define VIEWER_MENU_ABOUT         122
#endif

#if EREADER_MODEL == HANLIN_V5
#define VIEWER_MENU_GOTOFIRSTPAGE 121
#define VIEWER_MENU_GOTOENDPAGE   122
#define VIEWER_MENU_GOTOINDEX     124
#define VIEWER_MENU_ABOUT         129
#endif

static struct viewer_menu_item_t libdjvu_menu[] = {
{VIEWER_MENU_GOTOFIRSTPAGE, NULL, NULL}, // delete "Go to first page" menu item
{VIEWER_MENU_GOTOENDPAGE, NULL, NULL},   // delete "Go to last page" menu item
{VIEWER_MENU_GOTOINDEX, NULL, NULL},     // delete "Go to index" menu item
{VIEWER_MENU_ABOUT, NULL, NULL},         // delete "About..." menu item
{DJVU_MENU_ABOUT, "DJVU_MENU_ABOUT", NULL},
{DJVU_MENU_ZOOMFACTOR_ENTER, "DJVU_MENU_ZOOMFACTOR_ENTER", NULL},
{DJVU_MENU_HSHIFT_ENTER, "DJVU_MENU_HSHIFT_ENTER", NULL},
{DJVU_MENU_VSHIFT_ENTER, "DJVU_MENU_VSHIFT_ENTER", NULL},
{DJVU_MENU_SHOW_WMARK, "DJVU_MENU_SHOW_WMARK", NULL},
{DJVU_MENU_MULTICOL, "DJVU_MENU_MULTICOL", NULL},
{DJVU_MENU_HELP, "DJVU_MENU_HELP", NULL},
{0, NULL, NULL}
};

const struct viewer_menu_item_t *GetCustomViewerMenu(void)
{
    return libdjvu_menu;
}

static inline void paint_white_block(void)
{
    v3_callbacks->BeginDialog();
    v3_callbacks->BlitBitmap(INPUT_CURSOR_X, INPUT_CURSOR_Y, INPUT_BLOCK_WIDTH, INPUT_BLOCK_HEIGHT, 0, 0, INPUT_BLOCK_WIDTH, INPUT_BLOCK_HEIGHT, whiteblock);
    v3_callbacks->SetFontSize(24);
    v3_callbacks->PartialPrint();
}

#define HELP_STARTX 17
#define HELP_STARTY 35
#define HELP_STEPY  33

char *get_local_string(char *name)
{
    return v3_callbacks->GetString(name) ? : name;
}

static inline void print_help_line(int y, char *name)
{
    char *msg = get_local_string(name);
    v3_callbacks->TextOut(HELP_STARTX, y, msg, strlen(msg), TF_UTF8);
}

static inline void draw_help_frame(void)
{
    v3_callbacks->Rect(2, 2, SCREEN_WIDTH-4, SCREEN_HEIGHT-4);
    v3_callbacks->Rect(5, 5, SCREEN_WIDTH-9, SCREEN_HEIGHT-9);
    v3_callbacks->Rect(8, 8, SCREEN_WIDTH-14, SCREEN_HEIGHT-14);
}

static inline void paint_help_screen(void)
{
    int y = HELP_STARTY;

    v3_callbacks->BeginDialog();
    v3_callbacks->ClearScreen(0xFF);
    draw_help_frame();
    v3_callbacks->SetFontSize(28);
    print_help_line(y, "DJVU_MENU_HELP_TITLE");
    v3_callbacks->SetFontSize(24);
    print_help_line(y += HELP_STEPY + 10, "DJVU_MENU_HELP_PLUS");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONGPLUS");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_MINUS");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONGMINUS");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_1_4");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_5");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONG5");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_6");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONG6");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_7");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_8");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONG8");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_9");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONG9");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_0");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONG0");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_RIGHT");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONGRIGHT");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LEFT");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONGLEFT");
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_LONGOK");
    v3_callbacks->SetFontSize(22);
    print_help_line(y += HELP_STEPY, "DJVU_MENU_HELP_PRESSANYKEY");
    v3_callbacks->Print();
}

#define ABOUT_STARTX 17
#define ABOUT_STARTY 10
#define ABOUT_STEPY  40

static void gui_printf(int y, const char *fmt, ...)
{
    static char buf[1024];
    va_list args;
    va_start(args, fmt);
    (void)vsprintf(buf, fmt, args);
    va_end(args);
    v3_callbacks->TextOut(ABOUT_STARTX, y, buf, strlen(buf), TF_UTF8);
}

static inline void paint_about_screen(void)
{
    int y = ABOUT_STARTY;
    char *date;

    v3_callbacks->BeginDialog();
    v3_callbacks->ClearScreen(0xFF);
    draw_help_frame();
    v3_callbacks->SetFontSize(24);

    gui_printf(y += ABOUT_STEPY,
        "%s: %s",
        get_local_string("DJVU_ABOUT_FILENAME"), base_file_name);

    gui_printf(y += ABOUT_STEPY,
        "%s: %s",
        get_local_string("DJVU_ABOUT_DIRNAME"), dir_name);

    gui_printf(y += ABOUT_STEPY,
        "%s: %d %s (%.1f%s)",
        get_local_string("DJVU_ABOUT_FILESIZE"), file_stat.st_size, get_local_string("DJVU_ABOUT_BYTES"),
        (float)(file_stat.st_size)/(1024*1024), get_local_string("DJVU_ABOUT_MB"));

    date = ctime(&file_stat.st_mtime);
    date[strlen(date)-1] = '\0'; // get rid of terminating '\n'
    gui_printf(y += ABOUT_STEPY,
        "%s: %s",
        get_local_string("DJVU_ABOUT_DATE"), date);

    gui_printf(y += ABOUT_STEPY,
        "%s: %s, %d %s",
        get_local_string("DJVU_ABOUT_DJVUDOC"), get_djvu_doc_type(),
        numpages, get_local_string("DJVU_ABOUT_PAGES"));

    gui_printf(y += ABOUT_STEPY,
        "%s %d: %s %dx%d %ddpi v%d (Lib v%d)",
        get_local_string("DJVU_ABOUT_PAGE"), page_number + 1, get_djvu_page_type(),
        page_width, page_height, ddjvu_page_get_resolution(djvu_page),
        ddjvu_page_get_version(djvu_page), ddjvu_code_get_version());

    gui_printf(y += ABOUT_STEPY,
        "%s: %s",
        get_local_string("DJVU_ABOUT_RENDMODE"), get_djvu_render_mode());

    gui_printf(y += ABOUT_STEPY,
        "%s: %dms%s, %s: %dms",
        get_local_string("DJVU_ABOUT_DECODE"), page_decode_time_ms,
        page_decode_time_ms ? "" : get_local_string("DJVU_ABOUT_CACHED"),
        get_local_string("DJVU_ABOUT_RENDER"), page_render_time_ms);

    gui_printf(y += ABOUT_STEPY,
        "%s: %ldMB, %s: %s",
        get_local_string("DJVU_ABOUT_DJVUCACHE"), ddjvu_cache_get_size(djvu_context)/(1024*1024),
        get_local_string("DJVU_ABOUT_MULTICOL"),
        multicol ? get_local_string("DJVU_ABOUT_ON") : get_local_string("DJVU_ABOUT_OFF"));

    gui_printf(y += ABOUT_STEPY,
        "%s: %s, %s: %s",
        get_local_string("DJVU_ABOUT_ORIENT"),
        landscape ? get_local_string("DJVU_ABOUT_LANDSCAPE") : get_local_string("DJVU_ABOUT_PORTRAIT"),
        get_local_string("DJVU_ABOUT_WMARK"),
        show_wmark ? get_local_string("DJVU_ABOUT_ON") : get_local_string("DJVU_ABOUT_OFF"));

    gui_printf(y += ABOUT_STEPY,
        "%s: %.0f%%, %s: %d%%",
        get_local_string("DJVU_ABOUT_ZM"), 100*zoom_factor,
        get_local_string("DJVU_ABOUT_ZM_STEP"), zoom_factor_inc);

    gui_printf(y += ABOUT_STEPY,
        "%s: %d%%, %s: %d%%",
        get_local_string("DJVU_ABOUT_HORIZ_STEP"), horiz_shift_factor,
        get_local_string("DJVU_ABOUT_VERT_STEP"), vert_shift_factor);

    gui_printf(y += ABOUT_STEPY,
        "%s: %d%s",
        get_local_string("DJVU_ABOUT_SAVED_PAGE_NUMBER"), old_page_number + 1,
        old_page_number == -1 ? get_local_string("DJVU_ABOUT_NONE") : "");

    gui_printf(y += ABOUT_STEPY,
        "libdjvu %s Copyright (C) 2009 Tigran Aivazian", LIBDJVU_VERSION);

    v3_callbacks->SetFontSize(20);
    print_help_line(y += 2*ABOUT_STEPY, "DJVU_MENU_HELP_PRESSANYKEY");
    v3_callbacks->Print();
}

static inline void leave_menu_mode(void)
{
    v3_callbacks->BeginDialog();
    v3_callbacks->EndDialog();
}

int OnMenuAction(int action)
{
    int retval = 0;

    DPRINTF("%s(%d)\n", __FUNCTION__, action);

    switch (action) {
        case DJVU_MENU_ZOOMFACTOR_ENTER:
            min_input_value = MINZOOMSTEP;
            max_input_value = MAXZOOMSTEP;
            input_buffer = &zoom_factor_inc;
            paint_white_block();
            break;

        case DJVU_MENU_HSHIFT_ENTER:
            min_input_value = 1;
            max_input_value = MAXHSHIFT;
            input_buffer = &horiz_shift_factor;
            paint_white_block();
            break;

        case DJVU_MENU_VSHIFT_ENTER:
            min_input_value = 1;
            max_input_value = MAXVSHIFT;
            input_buffer = &vert_shift_factor;
            paint_white_block();
            break;

        case DJVU_MENU_SHOW_WMARK:
            show_wmark = 1 - show_wmark;
            retval = 1;
            break;

        case DJVU_MENU_MULTICOL:
            multicol = 1 - multicol;
            retval = 1;
            break;

        case DJVU_MENU_ABOUT:
            paint_about_screen();
            waiting_for_a_key = 1;
            break;

        case DJVU_MENU_HELP:
            paint_help_screen();
            waiting_for_a_key = 1;
            break;
    }

    if (retval) leave_menu_mode();
    return retval;
}

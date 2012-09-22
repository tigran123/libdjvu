/*
 * bookmarks.c Outline (aka "bookmarks") manipulation for libdjvu
 */

#include <stdio.h>
#include <string.h>

#include <libdjvu/miniexp.h>
#include <libdjvu/ddjvuapi.h>

#include "bookmarks.h"
#include "debug.h"

miniexp_t outline;
miniexp_t g_outline_path[100];
int curLevel = -1;
unsigned short dirName[1024];
static char url[200];

static int str_utf82uni(unsigned char *utf8, int utf8len, unsigned short *org, int boundlen);

static inline void handle_ddjvu_messages(ddjvu_context_t *ctx)
{
    ddjvu_message_wait(ctx);
    while (ddjvu_message_peek(ctx))
        ddjvu_message_pop(ctx);
}

int iCreateDirList(void)
{
    DPRINTF("%s\n", __FUNCTION__);
    while ((outline = ddjvu_document_get_outline(djvu_document)) == miniexp_dummy)
        handle_ddjvu_messages(djvu_context);

    if (miniexp_listp(outline) &&
        (miniexp_length(outline) > 0) &&
        miniexp_symbolp( miniexp_nth(0, outline)) &&
        strcmp(miniexp_to_name(miniexp_nth(0, outline)), "bookmarks") == 0)
    {
        DPRINTF("iCreateDirList success.\n");
        g_outline_path[0] = outline;
        curLevel = 0;
        return 1;
    }
    return 0;
}

int iGetCurDirPage(int level, int idx)
{
    DPRINTF("%s(%d,%d)\n", __FUNCTION__, level, idx);

    if (curLevel < 0) return 0;
    int pos = level;
    if (curLevel == 0) pos++;
    else pos += 2;

    miniexp_t cur = miniexp_nth(pos, g_outline_path[curLevel]);

    if (miniexp_consp(cur) && (miniexp_length(cur) > 0) &&
        miniexp_stringp(miniexp_nth(0, cur)) &&
        miniexp_stringp(miniexp_nth(1, cur)))
    {
        strcpy(url, miniexp_to_str(miniexp_nth(1, cur)));
        DPRINTF("url: %s\n", url);
        if(url[0] == '#')
            return atoi(url + 1) - 1;
    }
    return 0;
}

int iGetDirNumber(void)
{
    int l = miniexp_length(g_outline_path[curLevel]);

    DPRINTF("%s(%d)\n", __FUNCTION__, l);

    if(curLevel == 0) l--;
    else l -= 2;
    return l;   
}

unsigned short *usGetCurDirNameAndLen(int pos, int *len)
{
    char utf8Buf[2048];

    if (curLevel < 0) return 0;
    if (curLevel == 0) pos++;
    else pos += 2;

    miniexp_t cur = miniexp_nth(pos, g_outline_path[curLevel]);
    if (miniexp_consp(cur) && (miniexp_length(cur) > 0) &&
        miniexp_stringp(miniexp_nth(0, cur)) &&
        miniexp_stringp(miniexp_nth(1, cur)))
    {
        strcpy(utf8Buf, miniexp_to_str(miniexp_nth(0, cur)));
        *len = str_utf82uni((unsigned char *)utf8Buf, strlen(utf8Buf), dirName, 1024);
        DPRINTF("%s(%d,*%d)\n", __FUNCTION__, pos, *len);
        return dirName;
    }
    return NULL;
}

int bCurItemIsLeaf(int pos)
{
    DPRINTF("%s\n", __FUNCTION__);

    if (curLevel < 0) return 0;
    if (curLevel == 0) pos++;
    else pos += 2;
    miniexp_t cur = miniexp_nth(pos, g_outline_path[curLevel]);

    //>2 is skip title and destination
    if (miniexp_consp(cur) && (miniexp_length(cur) > 2))
        return 0;

    return 1;
}

void vEnterChildDir(int pos)
{
    DPRINTF("%s\n", __FUNCTION__);

    if (curLevel == 0) pos++; //skip bookmarks
    else pos += 2; //skip title and destination

    miniexp_t cur = miniexp_nth(pos, g_outline_path[curLevel]);
    if (cur)
        g_outline_path[++curLevel] = cur;
}

void vReturnParentDir(void)
{
    DPRINTF("%s\n", __FUNCTION__);
    if(curLevel > 0)
        curLevel--;
}

struct utf8_table {
    int     cmask;
    int     cval;
    int     shift;
    long    lmask;
    long    lval;
};

static struct utf8_table utf8_table[] =
{
    {0x80,  0x00,   0*6,    0x7F,           0,         /* 1 byte sequence */},
    {0xE0,  0xC0,   1*6,    0x7FF,          0x80,      /* 2 byte sequence */},
    {0xF0,  0xE0,   2*6,    0xFFFF,         0x800,     /* 3 byte sequence */},
    {0xF8,  0xF0,   3*6,    0x1FFFFF,       0x10000,   /* 4 byte sequence */},
    {0xFC,  0xF8,   4*6,    0x3FFFFFF,      0x200000,  /* 5 byte sequence */},
    {0xFE,  0xFC,   5*6,    0x7FFFFFFF,     0x4000000, /* 6 byte sequence */},
    {0,                            /* end of table    */}
};

int utf8_wctomb(unsigned char *s, wchar_t wc, int maxlen)
{
    long l;
    int c, nc;
    struct utf8_table *t;

    if (s == 0)
        return 0;

    l = wc;
    nc = 0;
    for (t = utf8_table; t->cmask && maxlen; t++, maxlen--) {
        nc++;
        if (l <= t->lmask) {
            c = t->shift;
            *s = t->cval | (l >> c);
            while (c > 0) {
                c -= 6;
                s++;
                *s = 0x80 | ((l >> c) & 0x3F);
            }
            return nc;
        }
    }
    return -1;
}

int utf8_mbtowc(unsigned short *p, const unsigned char *s, int n)
{
    long l;
    int c0, c, nc;
    struct utf8_table *t;

    nc = 0;
    c0 = *s;
    l = c0;
    for (t = utf8_table; t->cmask; t++) {
        nc++;
        if ((c0 & t->cmask) == t->cval) {
            l &= t->lmask;
            if (l < t->lval)
                return -1;
            *p = l;
            return nc;
        }
        if (n <= nc)
            return -1;
        s++;
        c = (*s ^ 0x80) & 0xFF;
        if (c & 0xC0)
            return -1;
        l = (l << 6) | c;
    }
    return -1;
}


int uni2utf8(wchar_t uni, unsigned char *out, int boundlen)
{
    int n;

    if ((n = utf8_wctomb(out, uni, boundlen)) == -1) {
        *out = '?';
        return -1;
    }
    return n;
}

int utf82uni(const unsigned char *rawstring, int boundlen, unsigned short *uni)
{
    int n;

    if ((n = utf8_mbtowc(uni, rawstring, boundlen)) == -1) {
        *uni = 0x003f;  /* ? */
        n = -1;
    }
    return n;
}

int str_uni2utf8(unsigned short *uni, int unilen, unsigned char *out, int boundlen)
{
    int ret, outlen = 0;

    while (unilen> 0 && boundlen > 0) {
        ret = uni2utf8(*uni, out, boundlen);
        if(ret == -1) break;
        unilen--;
        boundlen -= ret;
        uni++;
        out += ret;
        outlen += ret;
    }
    return outlen;
}

static int str_utf82uni(unsigned char *utf8, int utf8len, unsigned short *org, int boundlen)
{
    int ret, outlen = 0;
    unsigned short *out = org;

    while (utf8len> 0 && boundlen > 0) {
        ret = utf82uni(utf8, boundlen, out);
        if(ret == -1) break;
        utf8len -= ret;
        utf8 += ret;
        boundlen--;
        out++;
        outlen++;
    }
    return outlen;
}



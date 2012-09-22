/*
 * id2string.c - convert numeric ids to human-readable text. Part of libdjvu.
 *
 */

#include <libdjvu/ddjvuapi.h>
#include "id2string.h"

static const char *ptr;

const char *get_djvu_doc_type(void)
{
    ddjvu_document_type_t djvu_doc_type = ddjvu_document_get_type(djvu_document);
    switch (djvu_doc_type) {
        case DDJVU_DOCTYPE_UNKNOWN:
            ptr = "UNKNOWN";
            break;
        case DDJVU_DOCTYPE_SINGLEPAGE:
            ptr = "SINGLE PAGE";
            break;
        case DDJVU_DOCTYPE_BUNDLED:
            ptr = "BUNDLED";
            break;
        case DDJVU_DOCTYPE_INDIRECT:
            ptr = "INDIRECT";
            break;
        case DDJVU_DOCTYPE_OLD_BUNDLED:
            ptr = "OLD BUNDLED (OBSOLETE)";
            break;
        case DDJVU_DOCTYPE_OLD_INDEXED:
            ptr = "OLD INDEXED (OBSOLETE)";
            break;
        default:
            ptr = "UNDETERMINED";
            break;
    }
    return ptr;
}

const char *get_djvu_page_type(void)
{
    switch (page_type) {
        case DDJVU_PAGETYPE_UNKNOWN:
            ptr = "UNKNOWN";
            break;
        case DDJVU_PAGETYPE_BITONAL:
            ptr = "BITONAL";
            break;
        case DDJVU_PAGETYPE_PHOTO:
            ptr = "PHOTO";
            break;
        case DDJVU_PAGETYPE_COMPOUND:
            ptr = "COMPOUND";
            break;
        default:
            ptr = "UNDETERMINED";
            break;
    }
    return ptr; 
}

const char *get_djvu_render_mode(void)
{
    switch (djvu_render_mode) {
        case DDJVU_RENDER_COLOR:
             if (user_djvu_render_mode)
                 ptr = "COLOUR, THEN B&W (USER)";
             else
                 ptr = "COLOUR, THEN B&W (AUTO)";
             break;
        case DDJVU_RENDER_BLACK:
             if (user_djvu_render_mode)
                 ptr = "B&W, THEN COLOUR (USER)";
             else
                 ptr = "B&W, THEN COLOUR (AUTO)";
             break;
        case DDJVU_RENDER_COLORONLY:
             if (user_djvu_render_mode)
                 ptr = "COLOUR ONLY (USER)";
             else
                 ptr = "COLOUR ONLY (AUTO)";
             break;
        case DDJVU_RENDER_MASKONLY:
             if (user_djvu_render_mode)
                 ptr = "B&W ONLY (USER)";
             else
                 ptr = "B&W ONLY (AUTO)";
             break;
        case DDJVU_RENDER_BACKGROUND:
             if (user_djvu_render_mode)
                 ptr = "BACKGROUND (USER)";
             else
                 ptr = "BACKGROUND (AUTO)";
             break;
        case DDJVU_RENDER_FOREGROUND:
             if (user_djvu_render_mode)
                 ptr = "FOREGROUND (USER)";
             else
                 ptr = "FOREGROUND (AUTO)";
             break;
        default:
            ptr = "UNDETERMINED";
            break;
    }
    return ptr;
}

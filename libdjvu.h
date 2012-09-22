#ifndef _LIBDJVU_H
#define _LIBDJVU_H

// in libdjvu.c
static inline int move_window_up(void);
static inline int move_window_down(void);
static inline int move_window_right(void);
static inline int move_window_left(void);

// in id2string.c
extern const char *get_djvu_render_mode(void);
extern const char *get_djvu_page_type(void);
extern const char *get_djvu_doc_type(void);

extern void vSetCurPage(int);
extern int bGetRotate(void);
extern void vSetRotate(int);

extern void vGetTotalPage(int *);
//used by browser
extern int Bigger(void);
extern int Smaller(void);
extern int Rotate(void);
extern int Fit(void);
extern int Prev(void);
extern int Next(void);
extern int GotoPage(int);
extern int Origin(void);

//used by main functionsa
extern void Release();
extern void GetPageDimension(int *, int *);
extern void SetPageDimension(int, int);
extern void SetPageDimension_l(int, int);
extern double dGetResizePro(void);
extern void vSetResizePro(double);
extern void GetPageData(void **);
extern int GetPageIndex(void);
extern int GetPageNum(void);
extern void bGetUserData(void **, int *);
extern void vSetUserData(void *, int);
/*word2text.cpp*/
extern int iGetDocPageWidth(void);
extern int iGetDocPageHeight(void);
extern int iGetRealPageRect(int *, int *, int *, int *, int *, int *);
extern int iGetRealPageRect2(int *, int *, int *, int *, int *, int *);

/*initDoc.cpp*/
extern unsigned short usGetLeftBarFlag(void);
extern void vEndInit(int);
extern void vEndDoc(void);
extern int InitDoc(char *);
extern int iInitDocF(char *, int, int);
extern void vFirstBmp(char *, int);

/* possible values of state passed to OnKeyPressed() */
enum {
    NORMALSTATE,    //context state
    MENUSTATE,      //meun state
    INPUTSTATE,     //page input state
    CATALOGSTATE,   //catalog state
    BOOKMARKSTATE,  //bookmark state
    ABOUTSTATE,     //about state
    CUSTOMIZESTATE  //customize state
};

/**************************************ctlcallback.cpp**********************************************/
/**
* Call this function on key press.
*
* keyId - id of key. Key codes should be defined somewhere in SDK header file.
* state - the viewer state while received the key
*
* If return value is 1, this means that key has been processed in plugin and viewer should flush the screen.
* If return value is 2, this means that key has been processed in plugin and no more processing is required.
* If return value is 0, or no such function defined in plugin, default processing should be done by Viewer.
*/
int OnKeyPressed(int keyId, int state);

struct CallbackFunction{
    void (*BeginDialog)();      //enter the customize mode
    void (*EndDialog)();        //exit the customize mode, viewer will flush the screen in it.
    void (*SetFontSize)(int fontSize);  
    void (*SetFontAttr)(int fontAttr);  //0, black font, 1, white font
    void (*TextOut)(int x, int y , char *text, int length, int flags);  //flag can be TF_ASCII , TF_UTF8 or TF_UC16
            //to ascii and utf8, length is the length of string.
            //to UC16, length is the length of 
    void (*BlitBitmap)(int x, int y, int w, int h, int src_x, int src_y, int src_width, int src_height, unsigned char *buf);
    void (*Line)(int x1, int y1, int x2, int y2);
    void (*Point)(int x, int y);
    void (*Rect)(int x, int y, int width, int height);
    void (*ReadArea)(int x, int y, int width, int height, unsigned char *save);
    void (*ClearScreen)(unsigned char color);   //0x0, black , 0xFF, white
    int  (*GetBatteryState)();
    int  (*GetLanguage)();
    char*(*GetString)(char* stringName);
    void (*Print)();                //full screen flush
    void (*PartialPrint)();     //partial screen flush
};

enum {TF_ASCII, TF_UTF8, TF_UC16};  

void SetCallbackFunction(struct CallbackFunction *cb);


/**
* Return 0 to hide standard statusbar, 1 to show it. If no such function defined in plugin, assume as 1.
*/
int IsStandardStatusBarVisible();

/******************************************MarkSearch.cpp,MarkSearch.h*********************************/
typedef struct status_info {
   int bookmarkLabelFlags; // bit set, (1, 2, 4, 8, 16) for bookmark icons 1, 2, 3, 4, 5 correspondingly
/*
*/
   int musicState;         // 0 no music, 1 music
   int batteryState;       // e.g. 0..16 for current energy level, -1 for charging mode.
   int currentBookmarkFlags; // bit set, (1,2,4,8,16) for bookmarks on current page (1,2,3,4,5)
} status_info_t;

typedef struct myrect_s {
    int x;
    int y;
    int width;
    int height;
} myrect;

/**
* Call when some status information is changed.
* Plugin should return 1 and write rectangle coordinates to rectToUpdate if it wants to update part of screen to show new status.
*/
int OnStatusInfoChange(status_info_t *, myrect *);

int OnMenuAction(int actionId);

struct viewer_menu_item_t {
    int actionId;
    const char *captionLabelId;
    struct viewer_menu_item_t *submenu;
};

const struct viewer_menu_item_t *GetCustomViewerMenu(void);
const char *GetAboutInfoText(void);

#endif 

#ifndef _BOOKMARKS_H
#define _BOOKMARKS_H

//extern void vGetCurDir(int, int);
extern int iGetCurDirPage(int, int);
extern int iCreateDirList(void);
extern int iGetDirNumber(void);
extern unsigned short* usGetCurDirNameAndLen(int, int *);
extern void vClearAllDirList(void);
extern int bCurItemIsLeaf(int);
extern void vEnterChildDir(int);
extern void vReturnParentDir(void);
extern void vFreeDir(void);

// in libdjvu.c
extern ddjvu_context_t *djvu_context;
extern ddjvu_document_t *djvu_document;

#endif 

#ifdef __arm__
#define KEY_BASE 30
#define KEY_0 (KEY_BASE)
#define KEY_1 (1+KEY_BASE)
#define KEY_2 (2+KEY_BASE)
#define KEY_3 (3+KEY_BASE)
#define KEY_4 (4+KEY_BASE)
#define KEY_5 (5+KEY_BASE)
#define KEY_6 (6+KEY_BASE)
#define KEY_7 (7+KEY_BASE)
#define KEY_8 (8+KEY_BASE)
#define KEY_9 (9+KEY_BASE)
#define KEY_BOOKMARK KEY_6          //alias name
#define KEY_CATALOG KEY_7
#define KEY_EXPANSION KEY_8
#define KEY_PREV KEY_9
#define KEY_NEXT KEY_0
#define KEY_CANCEL (10+KEY_BASE)
#define KEY_OK (11+KEY_BASE)
#define KEY_DOWN (12+KEY_BASE)     //page down key on the side
#define KEY_UP (13+KEY_BASE)   //page up key on the side
#define KEY_SHORTCUT_PREV KEY_UP    //shortcut key
#define KEY_SHORTCUT_NEXT KEY_DOWN
#define KEY_SHORTCUT_VOLUME_UP (14+KEY_BASE) 
#define KEY_SHORTCUT_VOLUME_DOWN (15+KEY_BASE)
#define KEY_POWEROFF (16+KEY_BASE)
#define KEY_CURSOR_UP (33+KEY_BASE) //prev key on the right
#define KEY_CURSOR_DOWN (34+KEY_BASE)   //next key on the right
#define KEY_CURSOR_OK (32+KEY_BASE) //OK

#define KEY_PREV_SONG KEY_9
#define KEY_NEXT_SONG KEY_0
#define KEY_FAST_FORWARD KEY_6
#define KEY_FAST_BACKWARD KEY_1
#define KEY_VOLUME_UP KEY_7
#define KEY_VOLUME_DOWN KEY_2
#define KEY_CIRCLE KEY_3
#define KEY_STYLE KEY_8

//key pressed for a long time
#define LONG_KEY_BASE 0x40+KEY_BASE
#define LONG_KEY_0 (LONG_KEY_BASE)
#define LONG_KEY_1 (1+LONG_KEY_BASE)
#define LONG_KEY_2 (2+LONG_KEY_BASE)
#define LONG_KEY_3 (3+LONG_KEY_BASE)
#define LONG_KEY_4 (4+LONG_KEY_BASE)
#define LONG_KEY_5 (5+LONG_KEY_BASE)
#define LONG_KEY_6 (6+LONG_KEY_BASE)
#define LONG_KEY_7 (7+LONG_KEY_BASE)
#define LONG_KEY_8 (8+LONG_KEY_BASE)
#define LONG_KEY_9 (9+LONG_KEY_BASE)
#define LONG_KEY_BOOKMARK LONG_KEY_6
#define LONG_KEY_CATALOG LONG_KEY_7
#define LONG_KEY_EXPANSION LONG_KEY_8
#define LONG_KEY_PREV LONG_KEY_9
#define LONG_KEY_NEXT LONG_KEY_0
#define LONG_KEY_CANCEL (10+LONG_KEY_BASE)
#define LONG_KEY_OK (11+LONG_KEY_BASE)
#define LONG_KEY_DOWN (12+LONG_KEY_BASE)     
#define LONG_KEY_UP (13+LONG_KEY_BASE)   
#define LONG_KEY_SHORTCUT_PREV LONG_KEY_UP  
#define LONG_KEY_SHORTCUT_NEXT LONG_KEY_DOWN
#define LONG_SHORTCUT_KEY_VOLUME_UP (14+LONG_KEY_BASE) 
#define LONG_SHORTCUT_KEY_VOLUME_DOWN (15+LONG_KEY_BASE)
#define LONG_KEY_POWEROFF (16+LONG_KEY_BASE)
#define LONG_KEY_CURSOR_UP (33+LONG_KEY_BASE)
#define LONG_KEY_CURSOR_DOWN (34+LONG_KEY_BASE)
#define LONG_KEY_CURSOR_OK (32+LONG_KEY_BASE)

#else
#define KEY_BASE 48
#define KEY_0 (KEY_BASE)
#define KEY_1 (1+KEY_BASE)
#define KEY_2 (2+KEY_BASE)
#define KEY_3 (3+KEY_BASE)
#define KEY_4 (4+KEY_BASE)
#define KEY_5 (5+KEY_BASE)
#define KEY_6 (6+KEY_BASE)
#define KEY_7 (7+KEY_BASE)
#define KEY_8 (8+KEY_BASE)
#define KEY_9 (9+KEY_BASE)
#define KEY_BOOKMARK KEY_6          
#define KEY_CATALOG KEY_7
#define KEY_EXPANSION KEY_8
#define KEY_PREV KEY_9
#define KEY_NEXT KEY_0
#define KEY_CANCEL 'n'
#define KEY_OK 'y'
#define KEY_UP ','     
#define KEY_DOWN '.'   
#define KEY_SHORTCUT_PREV KEY_UP
#define KEY_SHORTCUT_NEXT KEY_DOWN
#define KEY_SHORTCUT_VOLUME_UP 'u'
#define KEY_SHORTCUT_VOLUME_DOWN 'd' 
#define KEY_POWEROFF 'p'

#define KEY_PREV_SONG KEY_9
#define KEY_NEXT_SONG KEY_0
#define KEY_FAST_FORWARD KEY_6
#define KEY_FAST_BACKWARD KEY_1
#define KEY_VOLUME_UP KEY_7
#define KEY_VOLUME_DOWN KEY_2
#define KEY_CIRCLE KEY_3
#define KEY_STYLE KEY_8

//key pressed for a long time
#define LONG_KEY_0  ')'
#define LONG_KEY_1  '!'
#define LONG_KEY_2  '@'
#define LONG_KEY_3  '#'
#define LONG_KEY_4  '$'
#define LONG_KEY_5  '%'
#define LONG_KEY_6  '^'
#define LONG_KEY_7  '&'
#define LONG_KEY_8  '*'
#define LONG_KEY_9  '('
#define LONG_KEY_BOOKMARK LONG_KEY_6            //
#define LONG_KEY_CATALOG LONG_KEY_7
#define LONG_KEY_EXPANSION LONG_KEY_8
#define LONG_KEY_PREV LONG_KEY_9
#define LONG_KEY_NEXT LONG_KEY_0
#define LONG_KEY_CANCEL 'N'
#define LONG_KEY_OK 'Y'
#define LONG_KEY_UP '<'     //
#define LONG_KEY_DOWN '>'   //
#define LONG_KEY_SHORTCUT_PREV LONG_KEY_UP
#define LONG_KEY_SHORTCUT_NEXT LONG_KEY_DOWN
#define LONG_SHORTCUT_KEY_VOLUME_UP 'U'
#define LONG_SHORTCUT_KEY_VOLUME_DOWN 'D' 
#define LONG_KEY_POWEROFF 'P'

#endif

libdjvu --- DjVu Viewer Plugin for HanLin V3/V5 e-Readers
=========================================================


# HOW TO USE

Just open any .djvu file as usual and use the following keys:

ZOOM IN/OUT: Volume '+'/'-' keys
TRIPLE STEP ZOOM IN/OUT: Long press Volume '+'/'-' keys.
There are menu options for setting the zoom step.

ZOOM RESET TO DEFAULT (i.e. FIT WIDTH): '8' key.

HORIZONTAL MOVEMENT: Next/Prev buttons on the left side.
TRIPLE STEP HORIZONTAL MOVEMENT: Long press Next/Prev buttons on the left
side.
There are menu options for setting the horizontal and vertical
movement steps.

SAVE/RESTORE WINDOW STATE: Press '5'/Long '5'.

ROTATE: Long press 'OK'. Note that rotating resets the zoom level to default
zoom state.

CYCLE THROUGH ALL DJVU RENDERING MODES: Long press '8' key.
(This setting is persistent across page turns and saved/restored on document
close/open.)

FORCE NEXT/PREV PAGE TURN: Press/Long-press '6' key.

All settings are saved when closing the djvu file and restored on opening it.

# HOW TO COMPILE

* Step 1. Configure and build djvulibre-3.5.19:

```
$ wget http://downloads.sourceforge.net/djvu/djvulibre-3.5.19.tar.gz
$ tar xzf djvulibre-3.5.19.tar.gz
$ cd djvulibre-3.5.19
```

For i386:
```
$ ./configure --disable-djview --without-qt --disable-xmltools --disable-i18n
```

For arm
```
$ ./configure --host=arm-linux --disable-djview --without-qt --disable-xmltools --disable-i18n
```

Now compile djvulibre
```
$ make
```

* Step 2. Edit libdjvu Makefile to set architecture (ARCH)

```
$ cd ..
$ vi Makefile
```

Move djvulibre-3.5.19 to ../djvulibre-3.5.19-$(ARCH) as that is where the
Makefile expects to find it.

* Step 3. Build libdjvu.so:

```
$ make
```

* Step 4. Copy the file arm-lib/libdjvu.so to .lib directory on your Hanlin V3
reader's internal storage. This assumes that you are using the firmware which
contains the following line in its rofs/etc/profile:

```
export LD_LIBRARY_PATH=/home/.lib
```

such as the latest biv_sumy or tirwal versions.

Don't forget to append the content of msg/* files to the corresponding files
in root/language.

The original project page was http://sourceforge.net/projects/libdjvu/ but
I decided to move it to github.

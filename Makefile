#
# Makefile for building libdjvu.so
#

ARCH=arm
#ARCH=i386
MODEL=v5
DJVULIBREVERSION=3.5.19

ifeq ($(ARCH) , arm)
ifeq ($(MODEL) , v5)
CROSS = arm-linux-gnueabi-
else
PATH += :/arm-9tdmi-linux-gnu/gcc-3.3.4-glibc-2.2.5/bin/
CROSS = arm-linux-
endif
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
CFLAGS += -I../djvulibre-$(DJVULIBREVERSION)-$(ARCH) -Wall -pthread -DTHREADMODEL=POSIXTHREADS -DHAVE_CONFIG_H -D_XOPEN_SOURCE=600
LDFLAGS = -L$(ARCH)-lib-$(MODEL) -ldjvulibre

# Uncomment if building on x86_64
#ifeq ($(ARCH), i386)
#CFLAGS += -m32
#LDFLAGS += -m32
#endif

ifeq ($(MODEL) , v5)
CFLAGS += -DEREADER_MODEL=HANLIN_V5
else
CFLAGS += -DEREADER_MODEL=HANLIN_V3
endif

all: libdjvu.so

libdjvu.o: libdjvu.c libdjvu.h keyvalue.h debug.h
	$(CC) $< -fPIC $(CFLAGS) -c -o $@

bookmarks.o: bookmarks.c bookmarks.h debug.h
	$(CC) $< -fPIC $(CFLAGS) -c -o $@

id2string.o: id2string.c id2string.h
	$(CC) $< -fPIC $(CFLAGS) -c -o $@

libdjvu.so: libdjvu.o bookmarks.o id2string.o
	$(CC) --shared -fPIC $^ $(LDFLAGS) -o $@
	$(STRIP) $@
	cp $@ $(ARCH)-lib-$(MODEL)

clean:
	rm -rf *.o libdjvu.so

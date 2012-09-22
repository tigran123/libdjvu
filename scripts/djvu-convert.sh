#!/bin/bash

#
# djvu-convert.sh - Dump the pages and then re-encode them in JB2
# This may be useful for optimizing DjVu files for rendering on Hanlin V3 device.
#
# Author: Tigran Aivazian <tigran@bibles.org.uk>
# Thanks to "lunohod" of OpenInkpot fame for useful comments
#

# we assume that djvu filenames DO NOT CONTAIN SPACES
list=$(echo *.djvu)
rm -rf out tmp ; mkdir out
for file in $list
do
    mkdir tmp
    npages=$(djvused -e n $file)
    echo -n "Converting \"$file\" (${npages} pages): "
    djvused $file -e print-outline > tmp/$file.outline
    for p in `seq 1 $npages`
    do
        page=$(printf "%04d" $p)
        #pagespec=$(djvused -e ls $file | sed -ne "s/^[ \t]*$p P[ \t]*.* \(.*\)$/\1/p")
        #if djvudump $file  | sed -ne "/^[ \t]*FORM.*{$pagespec}/,/FORM/p" | grep -q "(color),[ \t]*.*x.*"
        #then
        #     ddjvu -format=pgm -mode=color -page=$p $file tmp/test$page.pgm
        #     c44 tmp/test$page.pgm tmp/test$page.djvu
        #else
             ddjvu -format=pbm -mode=color -page=$p $file tmp/test$page.pbm
             cjb2 -clean tmp/test$page.pbm tmp/test$page.djvu
        #fi
        rm tmp/test$page.p?m
    done
    djvm -c out/$file tmp/test*.djvu
    djvused out/$file -e "set-outline tmp/$file.outline ; save"
    rm -rf tmp
    echo "OK"
done

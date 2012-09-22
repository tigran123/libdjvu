#!/bin/bash

#
# pdf-to-djvu.sh - Convert Adobe PDF to B&W 600dpi DjVu
# Converts all (*.pdf) files in the current directory.
#

list=$(ls -1 *.pdf)

rm -rf tmp out ; mkdir out
for file in $list
do
   mkdir tmp
   echo -n "Converting \"$file\": "
   cd tmp
   pdftoppm -mono -r 600 ../"$file" page > /dev/null 2>&1
   images=$(echo page*.pbm)
   for image in $images
   do
      base=$(echo $image | cut -d'.' -f1)
      cjb2 -clean $image $base.djvu
   done
   djvu=$(echo $file | sed -e "s/pdf/djvu/g" -e "s/PDF/djvu/g")
   cd - > /dev/null 2>&1
   djvm -c out/$djvu tmp/*.djvu
   rm -rf tmp
   echo "OK"
done

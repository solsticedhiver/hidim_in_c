#!/bin/sh
src=(hidim.c hidim.h CMakeLists.txt readme.txt)
files=""
for i in ${src[*]}
do
	files="$files hid.im/$i"
done
tar czvf hidim.tar.gz -C .. $files


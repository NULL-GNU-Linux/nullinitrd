#!/bin/sh

case "$PWD" in
    */scripts) echo "Run this from the toplevel dir of the source." ; exit 1 ;;
    *) ;;
esac

VERSION=$(cat Version)

make clean
make -j$(nproc)
make install DESTDIR=$(pwd)/package/stage
echo "You have downloaded nullinitrd version $VERSION. Copy files from stage/ to install." > package/README
fakeroot tar -I gzip -cf nullinitrd.tar.gz package/
rm -rf package/

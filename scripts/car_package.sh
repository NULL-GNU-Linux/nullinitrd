#!/bin/sh

case "$PWD" in
    */scripts) echo "Run this from the toplevel dir of the source." ; exit 1 ;;
    *) ;;
esac

VERSION=$(cat Version)

make clean
make -j$(nproc)
make install DESTDIR=$(pwd)/package
echo "version $VERSION" > package/car
fakeroot tar -I zstd -cf nullinitrd.tar.zst package/
rm -rf package/

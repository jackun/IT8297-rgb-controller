#!/bin/zsh

OUT=rgblights.macos
LIBUSB_VER=1.0.23

if [ ! -d /usr/local/Cellar/libusb/$LIBUSB_VER/include/libusb-1.0 ];then
    if [ ! -f $(which brew) ]; then
        echo "Get brew from https://brew.sh (and install libusb)"
        exit 1
    else
        echo Installing libusb...
        brew install libusb
    fi
fi

echo Building...
llvm-g++ -o $OUT rgblights/main.cpp -std=c++11 -lusb-1.0 \
       -I/usr/local/Cellar/libusb/$LIBUSB_VER/include/libusb-1.0 && \
       echo OK. Run it with \"./$OUT\" || echo failed

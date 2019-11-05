#!/bin/zsh

OUT=rgblights.macos

if [ ! -f $(which brew) ]; then
    echo "Get brew from https://brew.sh (and install hidapi and pkgconfig)"
    exit 1
fi

brew list hidapi pkgconfig 2>/dev/null 1>/dev/null
if [ $? -ne 0 ] ; then
    echo Installing hidapi and pkgconfig...
    # pkg-config might need opening a new shell (updated $PATH)?
    brew install hidapi pkgconfig
fi

echo Building...
llvm-g++ -o $OUT rgblights/rgblights.cpp Demo/Demo.cpp -I./rgblights \
    -std=c++11 -DHAVE_HIDAPI=1 \
    $(pkg-config --cflags --libs hidapi) && \
    echo OK. Run it with \"./$OUT\" || echo failed

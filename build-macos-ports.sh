#!/bin/zsh

OUT=rgblights.macos

if [ ! -f $(which port) ]; then
    echo "Get MacPorts from https://www.macports.org (and install hidapi)"
    exit 1
fi

port info hidapi 2>/dev/null 1>/dev/null
if [ $? -ne 0 ] ; then
    echo Installing hidapi...
    sudo port install hidapi
fi

echo Building...
llvm-g++ -o $OUT rgblights/rgblights.cpp Demo/Demo.cpp -I./rgblights \
    -std=c++11 -DHAVE_HIDAPI=1 \
    $(/opt/local/bin/pkg-config --cflags --libs hidapi) && \
    echo OK. Run it with \"./$OUT\" || echo failed

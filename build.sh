g++ -g -o rgblights.lnx rgblights/main.cpp -DHAVE_LIBUSB=1 -Wfatal-errors $(pkg-config --cflags libusb-1.0) $(pkg-config --libs libusb-1.0)

g++ -g -o rgblights-hidapi-libusb.lnx rgblights/main.cpp -DHAVE_HIDAPI=1 -Wfatal-errors $(pkg-config --cflags hidapi-libusb) $(pkg-config --libs hidapi-libusb)

g++ -g -o rgblights-hidapi-hidraw.lnx rgblights/main.cpp -DHAVE_HIDAPI=1 -Wfatal-errors $(pkg-config --cflags hidapi-hidraw) $(pkg-config --libs hidapi-hidraw)

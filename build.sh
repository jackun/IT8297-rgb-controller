echo Building rgblights.lnx...
g++ -g -o rgblights.lnx rgblights/rgblights.cpp Demo/Demo.cpp Demo/dbusmgr.cpp -I./rgblights \
	-DHAVE_LIBUSB=1 -DHAVE_DBUS=1 -Wfatal-errors $(pkg-config --cflags --libs libusb-1.0) \
	$(pkg-config --cflags --libs dbus-1) -lpthread

echo Building rgblights-hidapi-libusb.lnx...
g++ -g -o rgblights-hidapi-libusb.lnx rgblights/rgblights.cpp Demo/Demo.cpp Demo/dbusmgr.cpp -I./rgblights \
	-DHAVE_HIDAPI=1 -DHAVE_DBUS=1 -Wfatal-errors $(pkg-config --cflags --libs hidapi-libusb) \
	$(pkg-config --cflags --libs dbus-1) -lpthread

echo Building rgblights-hidapi-hidraw.lnx...
g++ -g -o rgblights-hidapi-hidraw.lnx rgblights/rgblights.cpp Demo/Demo.cpp Demo/dbusmgr.cpp -I./rgblights \
	-DHAVE_HIDAPI=1 -DHAVE_DBUS=1 -Wfatal-errors $(pkg-config --cflags --libs hidapi-hidraw) \
	$(pkg-config --cflags --libs dbus-1) -lpthread

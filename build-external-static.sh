git submodule update --init

if [ ! -d ./external/libusb/bin ];then
	(cd external/libusb; ./autogen.sh --prefix=$(pwd)/bin --enable-static=yes; make install)
fi

g++ -o rgblights-static.lnx \
	rgblights/rgblights.cpp \
	Demo/Demo.cpp \
	-DHAVE_LIBUSB \
	-I./rgblights \
	-I./external/libusb/libusb \
	-L./external/libusb/bin/lib \
	-Wl,-Bstatic -lusb-1.0 \
	-Wl,-Bdynamic -ludev -lpthread

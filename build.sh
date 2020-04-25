set -e

echo Building rgblights.lnx...
g++ -g -o rgblights.lnx rgblights/rgblights.cpp Demo/Demo.cpp Demo/dbusmgr.cpp \
	Demo/LiquidColorGenerator.cpp Demo/PulseAudioSoundManager.cpp  Demo/SoundManagerBase.cpp -I./rgblights \
	-DHAVE_LIBUSB=1 -DHAVE_DBUS=1 -Wfatal-errors $(pkg-config --cflags --libs libusb-1.0 dbus-1 fftw3f libpulse) -lpthread

echo Building rgblights-hidapi-libusb.lnx...
g++ -g -o rgblights-hidapi-libusb.lnx rgblights/rgblights.cpp Demo/Demo.cpp Demo/dbusmgr.cpp \
	Demo/LiquidColorGenerator.cpp Demo/PulseAudioSoundManager.cpp  Demo/SoundManagerBase.cpp -I./rgblights \
	-DHAVE_HIDAPI=1 -DHAVE_DBUS=1 -Wfatal-errors $(pkg-config --cflags --libs hidapi-libusb dbus-1 fftw3f libpulse) -lpthread

exit $?

echo Building rgblights-hidapi-hidraw.lnx...
g++ -g -o rgblights-hidapi-hidraw.lnx rgblights/rgblights.cpp Demo/Demo.cpp Demo/dbusmgr.cpp \
	Demo/LiquidColorGenerator.cpp Demo/PulseAudioSoundManager.cpp  Demo/SoundManagerBase.cpp -I./rgblights \
	-DHAVE_HIDAPI=1 -DHAVE_DBUS=1 -Wfatal-errors $(pkg-config --cflags --libs hidapi-hidraw dbus-1 fftw3f libpulse) -lpthread

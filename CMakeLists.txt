CMAKE_MINIMUM_REQUIRED(VERSION 3.17.0)
SET(TargetName "rgblights")
PROJECT(${TargetName})
INCLUDE(FindPkgConfig)
INCLUDE(FindThreads)

SET(SRCS_RGB
	./rgblights/rgblights.cpp
)

SET(HDRS_RGB
	./rgblights/rgblights.h
)

SET(SRCS_DEMO
	./Demo/Demo.cpp
)

SET(HDRS_DEMO
	./Demo/dbusmgr.h
	./Demo/onsetsds/onsetsds.h
#	./Demo/BeatDetektor.h
	./Demo/libbeat/beatanalyser.h
	./Demo/libbeat/subband.h
	./Demo/libbeat/fft.h
	./Demo/cpu.h
)

IF(UNIX)
	LIST(APPEND SRCS_DEMO
		./Demo/dbusmgr.cpp
		./Demo/LiquidColorGenerator.cpp
		./Demo/SoundManagerBase.cpp
		./Demo/PulseAudioSoundManager.cpp
		./Demo/onsetsds/onsetsds.c
#		./Demo/BeatDetektor.cpp
		./Demo/libbeat/beatanalyser.cpp
		./Demo/libbeat/subband.cpp
		./Demo/libbeat/fft.cpp
        ./Demo/cpu.cpp
    )
ENDIF(UNIX)
IF(WIN32 OR MINGW)
	LIST(APPEND SRCS_DEMO
        ./Demo/cpu_win32.cpp
    )
ENDIF(WIN32 OR MINGW)


#IF(MSVC)
#	MESSAGE("Adding parallel build flag to CMAKE_CXX_FLAGS")
#	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
#	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX")
#	SOURCE_GROUP("Header Files" FILES ${HDRS})
#	SOURCE_GROUP("Source Files" FILES ${SRCS})
#ENDIF()

#FIND_PACKAGE(libusb-1.0 REQUIRED)
PKG_CHECK_MODULES(USB libusb-1.0)
PKG_CHECK_MODULES(PULSE libpulse)
PKG_CHECK_MODULES(DBUS dbus-1)
PKG_CHECK_MODULES(FFTW3 fftw3f)
PKG_CHECK_MODULES(HIDAPIRAW hidapi-hidraw)

INCLUDE_DIRECTORIES(./rgblights ${USB_INCLUDE_DIRS} ${PULSE_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS} ${FFTW3_INCLUDE_DIRS} ${HIDAPIRAW_INCLUDE_DIRS})
LIST(APPEND LIBS ${PULSE_LIBRARIES} ${DBUS_LIBRARIES} ${FFTW3_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

LIST(APPEND HDRS ${HDRS_RGB} ${HDRS_DEMO})
LIST(APPEND SRCS ${SRCS_RGB} ${SRCS_DEMO})

#IF(MINGW)
#	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -static-libgcc -static-libstdc++ -static -lpthread")
#ENDIF(MINGW)

IF(MINGW)

ELSEIF(UNIX)
	ADD_DEFINITIONS(-DHAVE_DBUS=1)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Wunused-variable -Wno-parentheses")
ENDIF(MINGW)

ADD_EXECUTABLE(${TargetName} ${SRCS_RGB} ${SRCS_DEMO})
TARGET_LINK_LIBRARIES(${TargetName} ${LIBS} ${USB_LIBRARIES})
TARGET_COMPILE_DEFINITIONS(${TargetName} PUBLIC HAVE_LIBUSB=1)

ADD_EXECUTABLE("${TargetName}-hidapi" ${SRCS_RGB} ${SRCS_DEMO})
TARGET_LINK_LIBRARIES("${TargetName}-hidapi" ${LIBS} ${HIDAPIRAW_LIBRARIES})
TARGET_COMPILE_DEFINITIONS("${TargetName}-hidapi" PUBLIC HAVE_HIDAPI=1)
SET_PROPERTY(TARGET ${TargetName} PROPERTY CXX_STANDARD 17)

# post-build copy for win32
#IF(WIN32 AND NOT MINGW)
#	ADD_CUSTOM_COMMAND( TARGET ${TargetName} PRE_BUILD
#		COMMAND if not exist \"${PROJECT_SOURCE_DIR}\"\\bin mkdir \"${PROJECT_SOURCE_DIR}\"\\bin )
#	ADD_CUSTOM_COMMAND( TARGET ${TargetName} POST_BUILD
#		COMMAND copy \"$<SHELL_PATH:$<TARGET_FILE:${TargetName}>>\" \"${PROJECT_SOURCE_DIR}/bin\")
#ENDIF(WIN32 AND NOT MINGW)

# set the executable output path for UNIX and MinGW instead of post-build copy
IF(MINGW OR UNIX)
	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
ENDIF(MINGW OR UNIX)

#IF(PACKAGING)
#	INSTALL(FILES "${CMAKE_SOURCE_DIR}/udev/99-it8297.rules" DESTINATION "/usr/share/doc/${TargetName}/udev/")
#ENDIF(PACKAGING)

#if _WIN32
#include <conio.h>
#endif

#include <thread>
#include <chrono>
#include <iostream>
#include <cstdio>
#include <cmath>
#include <string>
#include <iostream>
#include <libusb.h>

template < typename T, size_t N >
constexpr size_t countof(T(&arr)[N])
{
	return N;
}

uint32_t MakeColor(uint8_t r, uint8_t g, uint8_t b)
{
	return (r << 16) | (g << 8) | b;
}

const uint16_t VID = 0x048D;
const uint16_t PID = 0x8297;

// LED "headers" 0x20..0x27, As seen on Gigabyte X570 Elite board
const uint8_t HDR_IO_PORTS = 0x20;
const uint8_t HDR_AUD_CAPS = 0x23;
const uint8_t HDR_D_LED1 = 0x25;
const uint8_t HDR_D_LED2 = 0x26;
const uint8_t HDR_D_LED1_RGB = 0x58;
const uint8_t HDR_D_LED2_RGB = 0x59;

enum EffectType
{
	EFFECT_NONE = 0,
	EFFECT_STATIC = 1,
	EFFECT_PULSE = 2,
	EFFECT_FLASH = 3,
	EFFECT_COLORCYCLE = 4,
	// to be continued...
};

enum LEDCount
{
	LEDS_32 = 0,
	LEDS_64,
	LEDS_256,
	LEDS_512,
	LEDS_1024,
};

#pragma pack(push, 1)

union PktEffect
{
	unsigned char buffer[64];
	struct Effect
	{
		uint8_t report_id; // = 0xCC;
		uint8_t header; // = 0x20;
		uint32_t zone0; // rgb fusion seems to set it to pow(2, header - 0x20)
		uint32_t zone1;
		uint8_t reserved0;
		uint8_t effect_type; //?
		uint8_t max_brightness; //?
		uint8_t min_brightness;
		uint32_t color0;
		uint32_t color1;
		uint16_t period0; // fade in
		uint16_t period1; // fade out
		uint16_t period2; // hold
		uint16_t period3; // ???
		uint8_t effect_param0; //random speed???
		uint8_t effect_param1; //??
		uint8_t effect_param2; //idk, flash effect repeat count
		uint8_t effect_param3; //idk
		uint8_t padding0[30];
	} e;

	PktEffect()
	{
		Init(0);
	}

	void Init(int header)
	{
		memset(buffer, 0, sizeof(buffer));
		e.report_id = 0xCC;
		e.header = 32 + header; // set as default
		e.zone0 = pow(2, e.header - 32);
		e.effect_type = EFFECT_STATIC;
		e.max_brightness = 100;
		e.min_brightness = 0;
		e.color0 = 0x00FF2100; //orange
		e.period0 = 1200;
		e.period1 = 1200;
		e.period2 = 200;
		e.effect_param0 = 0;
		e.effect_param1 = 1;
	}
};

struct IT8297_Report
{
	uint8_t report_id;
	uint8_t product;
	uint8_t device_num;
	uint8_t total_leds;
	uint32_t fw_ver;
	uint16_t Strip_Ctrl_Length0;
	uint16_t reserved0;
	char str_product[32];
	uint32_t cal_strip0;
	uint32_t cal_strip1;
	uint32_t cal_strip2;
	uint32_t chip_id;
	uint32_t reserved1;
};

#pragma pack(pop)

/*
Device descriptor: 1812010002000000408d049782000101020001
String descriptor 0:
		''

String descriptor 1:
		''

String descriptor 2:
		''

Config descriptor 1: 09021b000101008032090400000003000000092110010001223615
bInterfaceNumber 0, bInterfaceClass 3
		HID report descriptor: 0689ff0910a1010903150025ff75089510b20201c0
		0x06, 0x89, 0xFF,  // Usage Page (Vendor Defined 0xFF89)
		0x09, 0x10,        // Usage (0x10)
		0xA1, 0x01,        // Collection (Application)
		0x09, 0x03,        //   Usage (0x03)
		0x15, 0x00,        //   Logical Minimum (0)
		0x25, 0xFF,        //   Logical Maximum (-1)
		0x75, 0x08,        //   Report Size (8)
		0x95, 0x10,        //   Report Count (16)
		0xB2, 0x02, 0x01,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile,Buffered Bytes)
		0xC0,              // End Collection
		// 21 bytes
*/

template<size_t buf_sz>
void dump(unsigned char (&buf)[buf_sz], int count)
{
	int i;
	// Dump results
	for (i = 0; i < count && i < (int)buf_sz; i++) {
		printf("%02x", buf[i]);
	}
	printf("\n");
}

unsigned char packet_cc6000[] = {
  0xcc, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Commit? Another effect doesn't seem to start to play without this
unsigned char packet_commit_effect[] = {
  0xcc, 0x28, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// beat?
unsigned char packet_cc3100[] = {
  0xcc, 0x31, 0x00 /* 1 - enable, 0 - disable */, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_cc3400[] = {
  0xcc, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//set leds color?
unsigned char packet_cc20ff[] = {
  0xcc, 0x20, 0xFF /* 0x00 fades to previous effect */,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x5a, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_cc2001[] = {
  0xcc, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x02, 0x5a, 0x00, 0x00, 0x21,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x04,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_cc2102[] = {
  0xcc, 0x21, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x64, 0x00, 0x00, 0x21,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_cc2204[] = {
  0xcc, 0x22, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x64, 0x00, 0x00, 0x21,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_cc2308[] = {
  0xcc, 0x23, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x64, 0x00, 0x00, 0x21,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_cc2410[] = {
  0xcc, 0x24, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x64, 0x00, 0x83, 0xcc,
  0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_static_orange[] = {
  0xcc, 0x25, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00,
  0x01,
  0x64, 0x00,
  0x00, 0x21, 0xff, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0xee, 0x02,
  0xf4, 0x01,
  0x00, 0x00,
  0x07, 0x01,
  0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//FF2100 double flash ~40 % speed
unsigned char packet_double_flash_40[] = {
  0xcc,
  0x25, 0x20, // bottom header
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x64, 0x00,
  0x00, 0x21, 0xff, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x64, 0x00,
  0x64, 0x00,
  0x98, 0x08,
  0x00, 0x00, 0x00, 0x01,
  0x06 /* flash count */, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_pulse_FF2100_upper[] = {
  0xcc,
  0x26, 0x40, // upper header
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x02, 0x5a, 0x00,
  0x00, 0x21, 0xff, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xb0, 0x04,
  0xb0, 0x04,
  0xf4, 0x01,
  0x00, 0x00,
  0x00, 0x01,
  0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_pulse_FF2100_bottom[] = {
  0xcc,
  0x25, 0x20, // bottom header
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x02, 0x5a, 0x00,
  0x00, 0x10, 0xFF, 0x00,
  0x00, 0x00, 0x21, 0x00,
  0x40, 0x07, //fade in period
  0xb0, 0x07, // fade out period
  0x04, 0x01, // hold period
  0x00, 0x00,
  0x00, 0x01,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_colorcycle_100[] = {
  0xcc, 0x25, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x04, 0x32, 0x00, 0x00, 0x00,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0x01,
  0x68, 0x01, 0xf4, 0x01, 0x00, 0x00, 0x07, 0x01,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char packet_colorcycle_0[] = {
  0xcc, 0x25, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x04, 0x64, 0x00, 0x00, 0x00,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x05,
  0xb0, 0x04, 0xf4, 0x01, 0x00, 0x00, 0x07, 0x01,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

class UsbDevice
{
public:
	virtual void Init() = 0;
	virtual int SendPacket(unsigned char *packet) = 0;
};

class UsbIT8297 : public UsbDevice
{
public:
	UsbIT8297()
	{
	}

	void Init()
	{
		int res = libusb_init(&ctx);
		if (res != LIBUSB_SUCCESS)
			throw std::exception("Failed to init libusb");

		handle = libusb_open_device_with_vid_pid(ctx, 0x048D, 0x8297);

		if (!handle)
			throw std::exception("Failed to open device");

		res = libusb_claim_interface(handle, 0);
		if (res != LIBUSB_SUCCESS)
			throw std::exception("Failed to claim interface 0");

		// Most of the start up sequence as RGB Fusion does it
		res = SendPacket(packet_cc6000);

		// get some HID report, should contain ITE stuff
		// FIXME vanilla libusb _hid_get_report over allocates buffer and IT8297 no likey
		res = libusb_control_transfer(handle, 0x21 | LIBUSB_ENDPOINT_IN, 0x01, 0x03CC, 0x0000, buffer, 64, 1000);
		if (res > 0) // max 32 byte string?
		{
			report = *reinterpret_cast<IT8297_Report *>(buffer);
			std::string prod(report.str_product, 32);
			std::cout << "Device: " << prod << std::endl;
			led_count = GetLedCount(report.total_leds);
		}

		memset(buffer, 0, 64);
		res = libusb_control_transfer(handle, 0x21 | LIBUSB_ENDPOINT_IN, 0x01, 0x60CC, 0x0000, buffer, 64, 1000);

		res = SendPacket(packet_cc3400); //set led count 0 aka 32
		res = SendPacket(packet_cc3100); //disable something
		//res = SendPacket(packet_cc2001); // set IO ports LED to pulse
	}

	// optionally stop effects
	void StopAll()
	{
		memset(buffer, 0, sizeof(buffer));
		buffer[0] = 0xCC;
		for (int i = 0; i < 8; i++)
		{
			buffer[1] = 0x20 + i;
			SendPacket(buffer);
		}
		SendPacket(packet_commit_effect);
		SendPacket(packet_cc3100);
		EnableEffect(true); // yeah...
		SendPacket(packet_cc20ff);
		SendPacket(packet_commit_effect);
	}

	~UsbIT8297()
	{
		if (ctx)
		{
			int res = libusb_release_interface(handle, 0);
			libusb_close(handle);
			libusb_exit(ctx);
			ctx = nullptr;
		}
	}

	int SendPacket(PktEffect &packet)
	{
		return SendPacket(packet.buffer);
	}

	int SendPacket(unsigned char *packet)
	{
		return libusb_control_transfer(handle, 0x21, 0x09, 0x03CC, 0x0000, packet, 64, 1000);
	}

	bool EnableEffect(bool enable)
	{
		memset(buffer, 0, sizeof(buffer));
		buffer[0] = 0xCC;
		buffer[1] = 0x32;
		buffer[2] = enable ? 0 : 1;
		return SendPacket(buffer) == 64;
	}

	bool SetLedCount(LEDCount i)
	{
		memset(buffer, 0, 64);
		buffer[0] = 0xCC;
		buffer[1] = 0x34;
		buffer[2] = i;
		return SendPacket(buffer) == 64;
	}

	bool StartEffect()
	{
		return SendPacket(packet_commit_effect) == 64;
	}

	void SetAllPorts(EffectType type, uint32_t color = 0)
	{
		PktEffect effect;
		for (int i = 0; i < 8; i++)
		{
			effect.Init(i);
			effect.e.effect_type = type;
			effect.e.color0 = color;
			SendPacket(effect);
		}
		StartEffect();
	}

private:

	uint32_t GetLedCount(uint32_t c)
	{
		switch (c)
		{
		case 0:
			return 32;
		case 1:
			return 64;
		case 2:
			return 256;
		case 3:
			return 512;
		case 4:
			return 1024;
		default:
			return 0;
		}
	}

	struct libusb_device_handle *handle = nullptr;
	struct libusb_context *ctx = nullptr;
	unsigned char buffer[64];
	IT8297_Report report;
	uint32_t led_count = 32;
};

struct RGBBytes
{
	uint8_t g;
	uint8_t r;
	uint8_t b;
};

union PktRGB
{
	unsigned char buffer[64];
	struct RGBData
	{
		uint8_t cc;// = 0xCC;
		uint8_t header;// = 0x58 - lower header, 0x59 - upper header;
		uint16_t boffset;// = 0; // in bytes, absolute
		uint8_t  bcount;// = 0;
		RGBBytes leds[19];
		uint16_t padding0;
	} s;

	PktRGB()
	{
		Reset();
	}

	void Reset()
	{
		s.cc = 0xCC;
		s.header = HDR_D_LED1_RGB; //sending as 0x53, or was it 0x54, screws with color cycle effect
		s.boffset = 0;
		s.bcount = 0;
		memset(s.leds, 0, sizeof(s.leds));
	}
};

void DoRGB(UsbDevice& usbDevice)
{
	int res;
	PktRGB packet;

	int delay_ms = 10;
	int led_offset = 0;

	int all_leds = 120; //defaults to 32 leds usually
	//for (int j = 0; j < 5000 / delay_ms; j++) // per 5 seconds or sumfin
	for (int j = 0; j < all_leds * 5; j++)
	{
		//for (int i = 0; i < countof(packet_bytes); i++)
		{
			int left_leds = all_leds;
			int leds = countof(packet.s.leds);
			int sent_data = 0;
			int k = 0;

			while (left_leds > 0) {
				packet.Reset();
				leds = min(leds, left_leds);
				left_leds -= leds;

				packet.s.bcount = leds * 3;
				packet.s.boffset = sent_data;
				sent_data += packet.s.bcount;

				/*for (int i = 0; i < leds; i++) {

					packet.s.leds[i].g = ((255 - (i * 0xFF / all_leds) + 1) + j*3) % 256; //G
					packet.s.leds[i].r = 255 - packet.s.leds[i].g; //R
					packet.s.leds[i].b = 0;// (packet.s.leds[i].r * packet.s.leds[i].g) % 32; //B
				}*/

				for (int i = 0; i < leds; i++) {
					if (led_offset == k) {
						packet.s.leds[i].r = 0x0F;
						std::cout << "set led @ " << led_offset << std::endl;
					}
					else if (led_offset == k + 1)
						packet.s.leds[i].g = 0x0F;
					else if (led_offset == k + 2)
						packet.s.leds[i].b = 0x0F;
					k++;
				}

				std::cout << "led offset " << (int)packet.s.boffset << ":" << (int)packet.s.bcount << std::endl;
				res = usbDevice.SendPacket(packet.buffer);
				if (res < 0) {
					std::cerr << "error: " << res << std::endl;
					return;
				}
			}

			led_offset = (led_offset + 1) % all_leds;
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

			std::cout << "line " << j << std::endl;
		}
	}
}


int main()
{
	UsbIT8297 ite;
	PktEffect effect;
	int res;
	struct libusb_device_handle *handle;

	try
	{
		ite.Init();
	}
	catch (std::exception &ex)
	{
		std::cerr << ex.what() << std::endl;
		return 1;
	}

	ite.SetLedCount(LEDS_256);
	ite.SetAllPorts(EFFECT_NONE);

	ite.EnableEffect(false);
	DoRGB(ite);
	ite.EnableEffect(true);

	ite.SetAllPorts(EFFECT_PULSE, MakeColor(0xFF, 0x21, 0));

#if _WIN32
	std::cerr << "\n\n\nPress enter to exit" << std::endl;
	//if (res < 0)
		_getch();
#endif

	//ite.StopAll();

	return 0;
}

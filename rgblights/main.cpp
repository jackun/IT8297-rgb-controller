#include <emmintrin.h>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <iostream>

using ms = std::chrono::milliseconds;

//#define HAVE_LIBUSB 1
//#define HAVE_HIDAPI 1

#if defined(HAVE_LIBUSB) || defined(HAVE_HIDAPI)
	#if defined(HAVE_LIBUSB)
		#include <libusb.h>
	#endif
	#if defined(HAVE_HIDAPI)
		#include <hidapi.h>
	#endif

	#if defined(HAVE_LIBUSB)
		class UsbIT8297_libusb;
		using UsbIT8297 = UsbIT8297_libusb;
	#elif defined(HAVE_HIDAPI)
		class UsbIT8297_hidapi;
		using UsbIT8297 = UsbIT8297_hidapi;
	#endif
#else
#error No backend defined. Define HAVE_LIBUSB or HAVE_HIDAPI.
#endif

#ifndef min
#define min std::min
#endif

bool pause = false;

#if _WIN32
#include <conio.h>
#include "Window.h"

bool running = true;

BOOL WINAPI consoleHandler(DWORD signal) {

	switch (signal) {
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_C_EVENT:
		running = false;
		break;
	default:
		break;
	}

	return TRUE;
}
#else
#include <signal.h>
volatile sig_atomic_t running = 1;
void sighandler(int sig) {
	if (sig == SIGINT)
		running = 0;
}
#endif

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

// TODO change order as needed
struct LEDs
{
	uint8_t g;
	uint8_t r;
	uint8_t b;
};

#pragma pack(push, 1)

union PktRGB
{
	unsigned char buffer[64];
	struct RGBData
	{
		uint8_t report_id;// = 0xCC;
		uint8_t header;// = 0x58 - lower header, 0x59 - upper header;
		uint16_t boffset;// = 0; // in bytes, absolute
		uint8_t  bcount;// = 0;
		LEDs leds[19];
		uint16_t padding0;
	} s;

	PktRGB(uint8_t hdr = HDR_D_LED1_RGB) : s{ 0 }
	{
		Reset(hdr);
	}

	void Reset(uint8_t hdr)
	{
		s.report_id = 0xCC;
		s.header = hdr; //sending as 0x53, or was it 0x54, screws with color cycle effect
		s.boffset = 0;
		s.bcount = 0;
		memset(s.leds, 0, sizeof(s.leds));
	}
};

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
		uint8_t effect_param0; //colorcycle - how many colors to cycle through (how are they set?)
					// flash - if >0 cycle through N colors
		uint8_t effect_param1; //??
		uint8_t effect_param2; //idk, flash effect repeat count
		uint8_t effect_param3; //idk
		uint8_t padding0[30];
	} e;

	PktEffect() : e { 0 }
	{
		Init(0);
	}

	void Init(int header)
	{
		memset(buffer, 0, sizeof(buffer));
		e.report_id = 0xCC;
		e.header = 32 + header; // set as default
		e.zone0 = (uint32_t)pow(2, e.header - 32);
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
	uint16_t curr_led_count;
	uint16_t reserved0;
	char str_product[32]; // might be 28 and an extra byteorder3
	uint32_t byteorder0;
	uint32_t byteorder1;
	uint32_t byteorder2;
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

class UsbIT8297Base
{
public:
	UsbIT8297Base()
	{
	}

	virtual ~UsbIT8297Base() {}

	virtual void Init() = 0;

	virtual int SendPacket(unsigned char *packet) = 0;

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
		ApplyEffect();
		EnableBeat(false);
		DisableEffect(false); // yeah...
		SendPacket(0x20, 0xFF); //?
		ApplyEffect();
	}

	int SendPacket(PktEffect &packet)
	{
		return SendPacket(packet.buffer);
	}

	bool SendPacket(uint8_t a, uint8_t b, uint8_t c = 0)
	{
		memset(buffer, 0, 64);
		buffer[0] = 0xCC;
		buffer[1] = a;
		buffer[2] = b;
		buffer[3] = c;
		return SendPacket(buffer) == 64;
	}

	bool DisableEffect(bool disable)
	{
		return SendPacket(0x32, disable ? 1 : 0);
	}

	bool SetLedCount(LEDCount s0 = LEDS_32, LEDCount s1 = LEDS_32)
	{
		return SendPacket(0x34, s0 | (s1 <<4));
	}

	// TODO is beat effect? beat lights leds and then fade out?
	bool EnableBeat(bool b)
	{
		return SendPacket(0x31, b ? 1 : 0);
	}

	bool ApplyEffect()
	{
		return SendPacket(0x28, 0xFF);
	}

	// FIXME doesn't save?
	bool SaveStateToMCU()
	{
		return SendPacket(0x5E, 0x00);
	}

	bool StartPulseOrFlash(bool pulseOrFlash = false, uint8_t hdr = 5, uint8_t colors = 0, uint8_t repeat = 1, uint16_t p0 = 200, uint16_t p1 = 200, uint16_t p2 = 2200, uint32_t color = 0)
	{
		PktEffect effect;
		effect.Init(hdr >= 0x20 ? hdr - 0x20 : hdr);
		effect.e.effect_type = pulseOrFlash ? EFFECT_FLASH : EFFECT_PULSE;
		effect.e.period0 = p0;
		effect.e.period1 = p1;
		effect.e.period2 = p2;
		effect.e.effect_param0 = colors;
		effect.e.effect_param1 = 0;
		effect.e.effect_param2 = repeat;
		int res = SendPacket(effect);
		return res == 64 && ApplyEffect();
	}

	bool SendRGB(const std::vector<uint32_t> &led_data, uint8_t hdr = HDR_D_LED1_RGB)
	{
		PktRGB packet;
		int sent_data = 0, res, k = 0;
		int leds = countof(packet.s.leds);
		int left_leds = led_data.size();

		while (left_leds > 0) {
			packet.Reset(hdr);
			leds = min(leds, left_leds);
			left_leds -= leds;

			packet.s.bcount = leds * 3;
			packet.s.boffset = sent_data;
			sent_data += packet.s.bcount;

			for (int i = 0; i < leds; i++) {
				uint32_t c = led_data[k];
				packet.s.leds[i].r = c & 0xFF;
				packet.s.leds[i].g = (c >> 8) & 0xFF;
				packet.s.leds[i].b = (c >> 16) & 0xFF;
				k++;
			}

			//std::cout << "led offset " << (int)packet.s.boffset << ":" << (int)packet.s.bcount << std::endl;
			res = SendPacket(packet.buffer);
			if (res < 0) {
				std::cerr << "error: " << res << std::endl;
				return false;
			}
		}
		return true;
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
		ApplyEffect();
	}

protected:

	// some basic startup sequences
	void Startup()
	{
		SetLedCount(LEDS_32);
		EnableBeat(false);
		SetAllPorts(EFFECT_PULSE, 0x00FF2100);
	}

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

	unsigned char buffer[64];
	IT8297_Report report;
	uint32_t led_count = 32;
};

#ifdef HAVE_LIBUSB
class UsbIT8297_libusb : public UsbIT8297Base
{
public:
	UsbIT8297_libusb()
	{
	}

	virtual void Init()
	{
		int res = libusb_init(&ctx);
		if (res != LIBUSB_SUCCESS)
			throw std::runtime_error("Failed to init libusb");

		handle = libusb_open_device_with_vid_pid(ctx, VID, PID);

		if (!handle)
			throw std::runtime_error("Failed to open device");

		if (libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER))
			libusb_set_auto_detach_kernel_driver(handle, 1);

		res = libusb_claim_interface(handle, 0);
		if (res != LIBUSB_SUCCESS)
			throw std::runtime_error("Failed to claim interface 0");

		// Most of the start up sequence as RGB Fusion does it
		// hid report read needs 0x60 packet or it gives io error. resets mcu or...?
		UsbIT8297Base::SendPacket(0x60, 0x00);

		// get some HID report, should contain ITE stuff
		// FIXME probably should be get_feature_report
		res = libusb_control_transfer(handle, 0x21 | LIBUSB_ENDPOINT_IN, 0x01, 0x03CC, 0x0000, buffer, 64, 1000);
		if (res > 0) // max 32 byte string?
		{
			report = *reinterpret_cast<IT8297_Report *>(buffer);
			std::string prod(report.str_product, 32);
			std::cout << "Device: " << prod << std::endl;
			led_count = GetLedCount(report.total_leds);
		}

		memset(buffer, 0, 64);
		buffer[0] = 0xCC;
		buffer[1] = 0x60;
		// get rgb calibration info?
		//res = libusb_control_transfer(handle, 0x21 | LIBUSB_ENDPOINT_IN, 0x01, 0x03CC, 0x0000, buffer, 64, 1000);

		Startup();
	}

	virtual ~UsbIT8297_libusb()
	{
		if (handle)
		{
			int res = libusb_release_interface(handle, 0);
			libusb_close(handle);
			handle = nullptr;
		}

		if (ctx)
		{
			libusb_exit(ctx);
			ctx = nullptr;
		}
	}

	virtual int SendPacket(unsigned char *packet)
	{
		return libusb_control_transfer(handle, 0x21, 0x09, 0x03CC, 0x0000, packet, 64, 1000);
	}

private:
	struct libusb_device_handle *handle = nullptr;
	struct libusb_context *ctx = nullptr;
};
#endif

#ifdef HAVE_HIDAPI
class UsbIT8297_hidapi : public UsbIT8297Base
{
public:
	UsbIT8297_hidapi()
	{
	}

	virtual void Init()
	{
		int res = hid_init();
		if (res < 0)
			throw std::runtime_error("Failed to init hid");

		hid_device_info *device_list = hid_enumerate(VID, PID);

		if (!device_list)
			throw std::runtime_error("No devices found");

		std::cerr << "Device path: " << device_list->path << std::endl;
		device = hid_open_path(device_list->path);
		hid_free_enumeration(device_list);

		if (!device)
			throw std::runtime_error("Failed to open device");

		// Most of the start up sequence as RGB Fusion does it
		// hid report read needs 0x60 packet or it gives io error. resets mcu or...?
		UsbIT8297Base::SendPacket(0x60, 0x00);

		// get some HID report, should contain ITE stuff
		memset(buffer, 0, 64);
		buffer[0] = 0xCC;
		res = hid_get_feature_report(device, buffer, sizeof(buffer));
		if (res > 0) // max 32 byte string?
		{
			report = *reinterpret_cast<IT8297_Report *>(buffer);
			std::string prod(report.str_product, 32);
			std::cerr << "Device: " << prod << std::endl;
			led_count = GetLedCount(report.total_leds);
		}

		Startup();
	}

	virtual ~UsbIT8297_hidapi()
	{
		if (device)
		{
			hid_close(device);
			device = nullptr;
		}
		hid_exit();
	}

	virtual int SendPacket(unsigned char *packet)
	{
		//return hid_write(device, packet, 64);
		return hid_send_feature_report(device, packet, 64);
	}

private:
	hid_device *device = nullptr;
};
#endif

/*! \brief Convert HSV to RGB color space

  Converts a given set of HSV values `h', `s', `v' into RGB
  coordinates. The output RGB values are in the range [0, 1], and
  the input HSV values are in the ranges h = [0, 360], and s, v =
  [0, 1], respectively.

  \param fR Red component, used as output, range: [0, 1]
  \param fG Green component, used as output, range: [0, 1]
  \param fB Blue component, used as output, range: [0, 1]
  \param fH Hue component, used as input, range: [0, 360]
  \param fS Hue component, used as input, range: [0, 1]
  \param fV Hue component, used as input, range: [0, 1]

*/
void HSVtoRGB(float& fR, float& fG, float& fB, float fH, float fS, float fV) {
	float fC = fV * fS; // Chroma
	float fHPrime = fmod(fH / 60.0, 6);
	float fX = fC * (1 - fabs(fmod(fHPrime, 2) - 1));
	float fM = fV - fC;

	if (0 <= fHPrime && fHPrime < 1) {
		fR = fC;
		fG = fX;
		fB = 0;
	}
	else if (1 <= fHPrime && fHPrime < 2) {
		fR = fX;
		fG = fC;
		fB = 0;
	}
	else if (2 <= fHPrime && fHPrime < 3) {
		fR = 0;
		fG = fC;
		fB = fX;
	}
	else if (3 <= fHPrime && fHPrime < 4) {
		fR = 0;
		fG = fX;
		fB = fC;
	}
	else if (4 <= fHPrime && fHPrime < 5) {
		fR = fX;
		fG = 0;
		fB = fC;
	}
	else if (5 <= fHPrime && fHPrime < 6) {
		fR = fC;
		fG = 0;
		fB = fX;
	}
	else {
		fR = 0;
		fG = 0;
		fB = 0;
	}

	fR += fM;
	fG += fM;
	fB += fM;

	//std::cout << "hue: " << fH << " " << fR << " " << fG << " " << fB << std::endl;
}

void DoRainbow(UsbIT8297& usbDevice)
{
	int repeat_count = 1;
	int delay_ms = 2;
	float hue = 0;
	int hue2 = 0;
	int hue_step = 1;
	float hue_stretch = .5f;
	int hue_offset = 0;
	float r = 0, g = 0, b = 0;
	bool dir = true;
	float pulse = 0.1f;
	float pulse_min = 0.1f;
	float pulse_speed = 0.01f;
	//defaults to 32 leds usually
	std::vector<uint32_t> led_data(120);

	while (running)
	{
		if (pause) {
			std::this_thread::sleep_for(ms(1500));
			continue;
		}
		//hue = hue2;
		for (size_t i = 0; i < led_data.size(); i++)
		{
			// FIXME ewww
			hue = ((360 - hue_offset) + (360.f / led_data.size()) * i * hue_stretch);
			hue2 = (int)hue % 360;

			HSVtoRGB(r, g, b, (float)hue2, 1.f, .25f);
			uint32_t c = ((uint32_t)(b * 255.f * pulse) << 16) | ((uint32_t)(g * 255.f * pulse) << 8) | (uint32_t)(r * 255.f * pulse);
			led_data[i] = c;
			//std::cout << "hue: " << hue2 << " " << hue << std::endl;
		}
		//hue2 = hue;

		if (!usbDevice.SendRGB(led_data))
			return;

		if (dir)
			pulse += pulse_speed;
		else
			pulse -= pulse_speed;

		if (pulse < pulse_min) {
			pulse = pulse_min;
			dir = true;
		}
		else if (pulse > 1.f) {
			pulse = 1;
			dir = false;
		}
		//std::cerr << pulse << std::endl;
		hue_offset = (hue_offset + hue_step) % 360;
		std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
	}
}

void DoRGB(UsbIT8297& usbDevice)
{
	int repeat_count = 1;
	auto delay = ms(10);
	size_t led_offset = 0;

	//defaults to 32 leds usually
	std::vector<uint32_t> led_data(120);

	const uint8_t step = led_data.size() / 60 /* roughly amount of lit leds */;
	const __m128i step128 = _mm_setr_epi8(step, step, step, 0, step, step, step, 0, step, step, step, 0, step, step, step, 0);

	while (running)
	{
		if (pause) {
			std::this_thread::sleep_for(ms(1500));
			continue;
		}
		for (int j = 0; j < led_data.size() * repeat_count; j++)
		{
			int i;

			if (!running)
				break;

			// snake effect, substract step from individual RGB color bytes
			size_t led_data_size = led_data.size();
			led_data_size -= led_data_size % 4;
			for (i = 0; i < led_data_size; i += 4)
			{
				//__m128i min_val = _mm_min_epu8(_mm_loadu_si128((const __m128i*)&led_data[i]), step128);
				//__m128i val = _mm_sub_epi8(_mm_loadu_si128((const __m128i*) & led_data[i]), min_val);
				__m128i val = _mm_subs_epu8(_mm_loadu_si128((const __m128i*) & led_data[i]), step128);
				_mm_storeu_si128((__m128i*) & led_data[i], val);

			}
			for (; i < led_data.size(); i++) //do unaligned left overs
			{
				uint32_t val = led_data[i];
				uint8_t r = val & 0xFF;
				uint8_t g = (val >> 8) & 0xFF;
				uint8_t b = (val >> 16) & 0xFF;
				r -= min(r, step);
				g -= min(g, step);
				b -= min(b, step);
				led_data[i] = r | (g << 8) | (b << 16);
			}

			//const uint32_t colors[] = { 0x00FF0000, 0x0000FF00, 0x000000FF };
			const uint32_t colors[] = { 0x00643264, 0x00643232, 0x00646432, 0x00326432, 0x00326464, 0x00323264 };
			led_data[led_offset] = colors[(led_offset % countof(colors))];

			if (!usbDevice.SendRGB(led_data))
				return;

			led_offset = (led_offset + 1) % led_data.size();
			std::this_thread::sleep_for(delay);
		}
	}
}

int main()
{
	PktEffect effect;
	UsbIT8297 ite;

#if _WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
		printf("\nERROR: Could not set control handler\n");
		return 1;
	}

	MyWindow win(100, 100);
	win.AddSuspendCB([&ite]() {
		pause = true;
		PktEffect pkt;
		pkt.Init(HDR_D_LED1);
		pkt.e.effect_type = EFFECT_STATIC;
		pkt.e.color0 = 0;
		ite.SendPacket(pkt);
		ite.ApplyEffect();
		ite.DisableEffect(false);
		std::cerr << "suspending" << std::endl;
	});
	win.AddResumeCB(
		[&ite]() {
		pause = false;
		ite.DisableEffect(true);
		std::cerr << "resumed" << std::endl;
	});

#else
	signal(SIGINT, sighandler);
#endif

	try
	{
		ite.Init();
	}
	catch (std::runtime_error &ex)
	{
		std::cerr << ex.what() << std::endl;
		return 1;
	}

	ite.SetLedCount(LEDS_256);
	for (int hdr=0; hdr<8; hdr++)
		ite.StartPulseOrFlash(false, hdr, 7, 2, 1200, 1200, 200);

	ite.DisableEffect(true);
	std::cerr << "CTRL + C to stop RGB loop" << std::endl;
	DoRainbow(ite);
	ite.DisableEffect(false);

#if _WIN32
	std::cerr << "\n\n\nPress enter to exit\nPress 's' to stop leds" << std::endl;
	//if (res < 0)
		int ch;
		FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

		ch = _getch();
		if (ch == 's')
			ite.StopAll();
#else
	ite.StopAll();
#endif

	return 0;
}

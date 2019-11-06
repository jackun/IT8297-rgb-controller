#pragma once

#if __cplusplus
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#else
#include <stdint.h>
#endif

#ifdef _WIN32
#define CALLBACK    __stdcall
#else
//32bit only? x64 g++ keeps telling __stdcall is ignored
#if UINTPTR_MAX == 0xffffffff
#define CALLBACK    __attribute__((stdcall,externally_visible,visibility("default")))
//#define CALLBACK    __attribute__((stdcall,visibility("default")))
#else
#define CALLBACK //__fastcall?
#endif
#endif

#ifndef EXPORT_C_
#if __cplusplus
#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" type CALLBACK
#else
#define EXPORT_C_(type) extern "C" CALLBACK type
#endif
#else
#define EXPORT_C_(type) type CALLBACK
#endif
#endif

struct libusb_device_handle;
struct libusb_context;

struct hid_device_;
typedef struct hid_device_ hid_device;

#if __cplusplus
namespace rgblights {
#endif

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
			struct LEDs leds[19];
			uint16_t padding0;
		} s;

#if __cplusplus
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
#endif

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

#if __cplusplus
		PktEffect() : e{ 0 }
		{
			Init(0);
		}

		void Init(int header)
		{
			memset(buffer, 0, sizeof(buffer));
			e.report_id = 0xCC;
			if (header < 8)
				e.header = 32 + header; // set as default
			else
				e.header = header;
			e.zone0 = (uint32_t)pow(2, e.header - 32);
			e.effect_type = EFFECT_STATIC;
			e.max_brightness = 100;
			e.min_brightness = 0;
			e.color0 = 0x00FF2100; //orange
			e.period0 = 1200;
			e.period1 = 1200;
			e.period2 = 200;
			e.effect_param0 = 0; // ex color count to cycle through (max seems to be 7)
			e.effect_param1 = 0;
			e.effect_param2 = 1; // ex flash repeat count
			e.effect_param3 = 0;
		}
#endif
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

//uint32_t MakeColor(uint8_t r, uint8_t g, uint8_t b);
static uint32_t MakeColor(uint8_t r, uint8_t g, uint8_t b)
{
	return (r << 16) | (g << 8) | b;
}

static LEDCount LedCountToEnum(uint32_t c)
{
	if (c <= 32)
		return LEDS_32;
	if (c <= 64)
		return LEDS_64;
	if (c <= 256)
		return LEDS_256;
	if (c <= 512)
		return LEDS_512;

	return LEDS_1024;
}

struct IT8297Device;

/* C exports for shared lib */
EXPORT_C_(struct IT8297Device*) create_device(/*ApiType api*/);
EXPORT_C_(void) free_device(struct IT8297Device* device);

#if __cplusplus

	template < typename T, size_t N >
	constexpr size_t countof(T(&arr)[N])
	{
		return N;
	}

	class UsbIT8297Base
	{
	public:
		UsbIT8297Base()
		{
		}

		virtual ~UsbIT8297Base() {}

		virtual void Init() = 0;

		virtual int SendPacket(unsigned char* packet) = 0;

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

		int SendPacket(PktEffect& packet)
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
			return SendPacket(0x34, s0 | (s1 << 4));
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

		bool StartPulseOrFlash(bool pulseOrFlash = false, uint8_t hdr = 5, uint8_t colors = 0, uint8_t repeat = 1, uint16_t p0 = 200, uint16_t p1 = 200, uint16_t p2 = 2200, uint32_t color = 0);
		bool SendRGB(const std::vector<uint32_t>& led_data, uint8_t hdr = HDR_D_LED1_RGB);
		void SetAllPorts(EffectType type, uint32_t color = 0);

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

		unsigned char buffer[64]{};
		IT8297_Report report{};
		uint32_t led_count = 32;
	};

#ifdef HAVE_LIBUSB
	class UsbIT8297_libusb : public UsbIT8297Base
	{
	public:
		using UsbIT8297Base::SendPacket;
		UsbIT8297_libusb()
		{
		}

		virtual void Init();
		virtual ~UsbIT8297_libusb();
		virtual int SendPacket(unsigned char* packet);
	private:
		struct libusb_device_handle* handle = nullptr;
		struct libusb_context* ctx = nullptr;
	};
#endif


#ifdef HAVE_HIDAPI
	class UsbIT8297_hidapi : public UsbIT8297Base
	{
	public:
		using UsbIT8297Base::SendPacket;
		UsbIT8297_hidapi()
		{
		}

		virtual void Init();
		virtual ~UsbIT8297_hidapi();
		virtual int SendPacket(unsigned char* packet);
	private:
		hid_device* device = nullptr;
	};
#endif

} //namespace
#endif

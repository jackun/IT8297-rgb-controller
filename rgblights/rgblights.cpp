#include "rgblights.h"
#include <string>
#include <iostream>

//#define HAVE_LIBUSB 1
//#define HAVE_HIDAPI 1

#if defined(HAVE_LIBUSB) || defined(HAVE_HIDAPI)
#if defined(HAVE_LIBUSB)
	#include <libusb.h>
#endif
#if defined(HAVE_HIDAPI)
	#include <hidapi.h>
#endif
#else
#error No backend defined. Define HAVE_LIBUSB or HAVE_HIDAPI.
#endif

namespace rgblights {

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
	void dump(unsigned char(&buf)[buf_sz], int count)
	{
		int i;
		// Dump results
		for (i = 0; i < count && i < (int)buf_sz; i++) {
			printf("%02x", buf[i]);
		}
		printf("\n");
	}


	bool UsbIT8297Base::StartPulseOrFlash(bool pulseOrFlash, uint8_t hdr, uint8_t colors, uint8_t repeat, uint16_t p0, uint16_t p1, uint16_t p2, uint32_t color)
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

	bool UsbIT8297Base::SendRGB(const std::vector<uint32_t>& led_data, uint8_t hdr)
	{
		PktRGB packet;
		int sent_data = 0, res, k = 0;
		int leds = countof(packet.s.leds);
		int left_leds = led_data.size();

		// Remap RGB to LED pins manually
		int byteorder = hdr == HDR_D_LED1_RGB ? report.byteorder0 : report.byteorder1;
		int bob, bog, bor;
		bob = byteorder & 0xFF;
		bog = (byteorder >> 8) & 0xFF;
		bor = (byteorder >> 16) & 0xFF;

		while (left_leds > 0) {
			packet.Reset(hdr);
			leds = (std::min)(leds, left_leds);
			left_leds -= leds;

			packet.s.bcount = leds * 3;
			packet.s.boffset = sent_data;
			sent_data += packet.s.bcount;

			for (int i = 0; i < leds; i++) {
				uint32_t c = led_data[k];
				packet.s.leds[i].b = (c >> (8 * (2 - bob))) & 0xFF;
				packet.s.leds[i].g = (c >> (8 * (2 - bog))) & 0xFF;
				packet.s.leds[i].r = (c >> (8 * (2 - bor))) & 0xFF;
				k++;
			}

			//std::cout << "led offset " << (int)packet.s.boffset << ":" << (int)packet.s.bcount << std::endl;
			res = SendPacket(packet.buffer);
			if (res < 0) {
				std::cerr << "SendPacket failed: " << res << std::endl;
				return false;
			}
		}
		return true;
	}

	void UsbIT8297Base::SetAllPorts(EffectType type, uint32_t color, uint32_t param0, uint32_t param2, uint32_t p0, uint32_t p1, uint32_t p2)
	{
		PktEffect effect;
		for (int i = 0; i < 8; i++)
		{
			effect.Init(i);
			effect.e.effect_type = type;
			effect.e.color0 = color;
			effect.e.effect_param0 = param0;
			effect.e.effect_param2 = param2;
			effect.e.period0 = p0;
			effect.e.period1 = p1;
			effect.e.period2 = p2;
			SendPacket(effect);
		}
		ApplyEffect();
	}

#ifdef HAVE_LIBUSB
	void UsbIT8297_libusb::Init()
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
		SendPacket(0x60, 0x00);

		// get some HID report, should contain ITE stuff
		// FIXME probably should be get_feature_report
		res = libusb_control_transfer(handle, 0x21 | LIBUSB_ENDPOINT_IN, 0x01, 0x03CC, 0x0000, buffer, 64, 1000);
		if (res > 0) // max 32 byte string?
		{
			report = *reinterpret_cast<IT8297_Report*>(buffer);
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

	UsbIT8297_libusb::~UsbIT8297_libusb()
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

	int UsbIT8297_libusb::SendPacket(unsigned char* packet)
	{
		if (!handle)
			return -1;
		return libusb_control_transfer(handle, 0x21, 0x09, 0x03CC, 0x0000, packet, 64, 1000);
	}
#endif

#ifdef HAVE_HIDAPI
	void UsbIT8297_hidapi::Init()
	{
		int res = hid_init();
		if (res < 0)
			throw std::runtime_error("Failed to init hid");

		hid_device_info* device_list = hid_enumerate(VID, PID);

		if (!device_list)
			throw std::runtime_error("No devices found");

		std::cerr << "Device path: " << device_list->path << std::endl;
		device = hid_open_path(device_list->path);
		hid_free_enumeration(device_list);

		if (!device)
			throw std::runtime_error("Failed to open device");

		// Most of the start up sequence as RGB Fusion does it
		// hid report read needs 0x60 packet or it gives io error. resets mcu or...?
		SendPacket(0x60, 0x00);

		// get some HID report, should contain ITE stuff
		memset(buffer, 0, 64);
		buffer[0] = 0xCC;
		res = hid_get_feature_report(device, buffer, sizeof(buffer));
		if (res > 0) // max 32 byte string?
		{
			report = *reinterpret_cast<IT8297_Report*>(buffer);
			std::string prod(report.str_product, 32);
			std::cerr << "Device: " << prod << std::endl;
			led_count = GetLedCount(report.total_leds);
		}

		Startup();
	}

	UsbIT8297_hidapi::~UsbIT8297_hidapi()
	{
		if (device)
		{
			hid_close(device);
			device = nullptr;
		}
		hid_exit();
	}

	int UsbIT8297_hidapi::SendPacket(unsigned char* packet)
	{
		if (!device)
			return -1;
		//return hid_write(device, packet, 64);
		return hid_send_feature_report(device, packet, 64);
	}
#endif

	struct IT8297Device {
		UsbIT8297Base* base;
	};

	EXPORT_C_(struct IT8297Device*) create_device(/*ApiType api*/)
	{
		IT8297Device* device = new IT8297Device();

#if defined(HAVE_LIBUSB)
		device->base = new UsbIT8297_libusb();
#elif defined(HAVE_HIDAPI)
		device->base = new UsbIT8297_hidapi();
#else
#error No backend
		return nullptr;
#endif

		try
		{
			device->base->Init();
		}
		catch (std::runtime_error & ex)
		{
			std::cerr << ex.what() << std::endl;
			free_device(device);
			return nullptr;
		}

		return device;
	}

	EXPORT_C_(void) free_device(struct IT8297Device* device)
	{
		if (device) {
			delete device->base;
			device->base = nullptr;
			delete device;
		}
	}
}

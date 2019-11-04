#include <emmintrin.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>
#include <rgblights.h>

using namespace rgblights;
using ms = std::chrono::milliseconds;

//#define HAVE_LIBUSB 1
//#define HAVE_HIDAPI 1

#if defined(HAVE_LIBUSB) || defined(HAVE_HIDAPI)
#if defined(HAVE_LIBUSB)
using UsbIT8297 = rgblights::UsbIT8297_libusb;
#elif defined(HAVE_HIDAPI)
using UsbIT8297 = rgblights::UsbIT8297_hidapi;
#endif
#else
#error No backend defined. Define HAVE_LIBUSB or HAVE_HIDAPI.
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
				r -= (std::min)(r, step);
				g -= (std::min)(g, step);
				b -= (std::min)(b, step);
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
	catch (std::runtime_error & ex)
	{
		std::cerr << ex.what() << std::endl;
		return 1;
	}

	ite.SetLedCount(LEDS_256);
	for (int hdr = 0; hdr < 8; hdr++)
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

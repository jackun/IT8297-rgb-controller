#include <emmintrin.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>
#include <sstream>
#include <rgblights.h>

#if _WIN32
#include "wgetopt.h"
#else
#include <unistd.h>
//#include <getopt.h>
#endif

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

#if defined(HAVE_DBUS)
#include "dbusmgr.h"
#endif

bool pause_loop = false;

#if _WIN32
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
	if (sig == SIGINT || sig == SIGTERM)
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

void DoRainbow(UsbIT8297& usbDevice, uint32_t led_count, LEDs& calib)
{
	int repeat_count = 1;
	int delay_ms = 16; // 16 - limit refresh to ~60fps, seems max is about 100 leds with libusb
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
	std::vector<uint32_t> led_data(led_count);

	while (running)
	{
		if (pause_loop) {
			std::this_thread::sleep_for(ms(1500));
			continue;
		}
		auto curr = std::chrono::high_resolution_clock::now();
		//hue = hue2;
		for (size_t i = 0; i < led_data.size(); i++)
		{
			// FIXME ewww
			hue = ((360 - hue_offset) + (360.f / led_data.size()) * i * hue_stretch);
			hue2 = (int)hue % 360;

			HSVtoRGB(r, g, b, (float)hue2, 1.f, 1.f);
			led_data[i] = (uint32_t)(r * 255.f * pulse * calib.r / 255)
				| (uint32_t)(g * 255.f * pulse * calib.g / 255) << 8
				| (uint32_t)(b * 255.f * pulse * calib.b / 255) << 16;
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
		auto dur = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - curr).count();
		//std::cerr << "Duration: " << dur << "ms, sleep " << std::max(delay_ms - dur, 0ll) << "ms" << std::endl;

		std::this_thread::sleep_for(std::chrono::milliseconds(std::max<int64_t>(delay_ms - dur, 0)));
	}
}

void DoRGB(UsbIT8297& usbDevice, uint32_t led_count, LEDs& calib)
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
		if (pause_loop) {
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

void ParseSetAllPorts(UsbIT8297Base& ite, const char * const opt)
{
	PktEffect effect;
	std::vector<uint32_t> params;
	std::stringstream ss(opt);

	std::string token;
	while (std::getline(ss, token, ',')) {
		params.push_back(std::stoi(token));
	}

	if (params.size() < 2) {
		std::cerr << "Failed to parse argument list." << std::endl;
		return;
	}

	ite.SetAllPorts(static_cast<EffectType>(params[0]), params[1]);
}

void ParseEffect(UsbIT8297Base& ite, const char * const opt)
{
	PktEffect effect;
	std::vector<uint32_t> params;
	std::stringstream ss(opt);

	std::string token;
	while (std::getline(ss, token, ',')) {
		params.push_back(std::stoi(token));
	}

	if (params.size() < 2) {
		std::cerr << "Failed to parse effect argument list." << std::endl; //TODO can happen?
		return;
	}

	std::cout << "Effect " << params[1] << " on header " << params[0] << std::endl;
	effect.Init(params[0]);

#define SETPARAM(n,i) \
	if (i < params.size()) effect.e.n = params[i]; \
	else { \
		std::cerr << "Missing argument for '" #n "'. Using defaults for this and rest." << std::endl; \
		goto breakout; }

	SETPARAM(effect_type, 1);
	SETPARAM(max_brightness, 2);
	SETPARAM(min_brightness, 3);

	SETPARAM(color0, 4);
	SETPARAM(color1, 5);

	SETPARAM(period0, 6);
	SETPARAM(period1, 7);
	SETPARAM(period2, 8);
	SETPARAM(period3, 9);

	SETPARAM(effect_param0, 10);
	SETPARAM(effect_param1, 11);
	SETPARAM(effect_param2, 12);
	SETPARAM(effect_param3, 13);

breakout:
#undef SETPARAM

	if (effect.e.effect_type == EFFECT_COLORCYCLE && effect.e.effect_param0 == 0)
		effect.e.effect_param0 = 7;
	// should probably call DisableEffect(false) for RGB headers
	ite.SendPacket(effect);
	ite.ApplyEffect();
}

void ParseCalib(LEDs& calib, const char * const opt)
{
	std::vector<uint32_t> params;
	std::stringstream ss(opt);

	std::string token;
	while (std::getline(ss, token, ',')) {
		params.push_back(std::stoi(token));
	}

	if (params.size() != 3) {
		std::cerr << "Too many/few arguments for calibration argument" << std::endl;
		return;
	}

	calib.r = (std::min)(params[0], 255u);
	calib.g = (std::min)(params[1], 255u);
	calib.b = (std::min)(params[2], 255u);
	std::cout << "Calibration: "
		<< (uint32_t)calib.r << ","
		<< (uint32_t)calib.g << ","
		<< (uint32_t)calib.b << std::endl;
}

void PrintUsage()
{
	std::cerr << "Usage:\n"
	"    Arguments are parsed in sequence.\n"
	"    Ex. use '-l 120 -a 2,16720128 -r -s' to set all ports to pulsing orange (0xFF2100), run RGB effect and then stop all effects after quiting.\n\n"
	"    -a <effect,color>\tset all ports to effect\n"
	"    -c <r,g,b>\tled calibration, value range is 0..255. Normalize rainbow effect to given range (basically max brightness)\n"
	"    -e <header,effect type[,other,params]>\t set built-in effect (comma separated arguments)\n"
	"    \theader\n"
	"    \t  32..39 (may depend on actual hardware)\n"
	"    \teffect type\n"
	"    \t  1 - static\n"
	"    \t  2 - pulse\n"
	"    \t  3 - flash\n"
	"    \t  4 - colorcycle\n"
	"    \t >4 - might have some more built-in effects\n"
	"    \tmax brightness\t (default 100)\n"
	"    \tmin brightness\t (default 0)\n\n"

	"    \tcolor 0\t- main effect color (in base-10 integer format)\n"
	"    \tcolor 1\n\n"

	"    \tperiod 0\t- ex fade in speed (default 1200)\n"
	"    \tperiod 1\t- ex fade out speed (default 1200)\n"
	"    \tperiod 2\t- ex hold period (default 200)\n"
	"    \tperiod 3\t- (default 0)\n\n"

	"    \teffect_param0\t- ex pulse/flash/colrocycle color count (max 7)\n"
	"    \teffect_param1\n"
	"    \teffect_param2\t- ex flash repeat count\n"
	"    \teffect_param3\n\n"

	"    -l <count>\t- LED count per strip\n"
	"    -r        \t- custom rainbow effect (needs preciding -l)\n"
	"    -s        \t- stop all effects\n"
	<< std::endl;
}

// if running custom effect (rainbow) switch LEDs on/off on computer sleep/resume events
void Suspend(UsbIT8297Base &ite)
{
	pause_loop = true;
	PktEffect pkt;
	pkt.Init(HDR_D_LED1);
	pkt.e.effect_type = EFFECT_STATIC;
	pkt.e.color0 = 0;
	ite.SendPacket(pkt);
	pkt.e.header = HDR_D_LED2;
	ite.SendPacket(pkt);
	ite.ApplyEffect();
	ite.DisableEffect(false);
	std::cerr << "suspending" << std::endl;
}

void Resume(UsbIT8297Base &ite)
{
	pause_loop = false;
	ite.DisableEffect(true);
	std::cerr << "resumed" << std::endl;
}

int main(int argc, char* const * argv)
{
	PktEffect effect;
	UsbIT8297 ite;
	uint32_t led_count = 32;
	const char * getopt_args = "a:c:rshl:e:";
	LEDs calib { 255, 255, 255 };
	int c;
	std::stringstream ss;

	// Pre-parse
	while ((c = getopt(argc, argv, getopt_args)) != -1)
	{
		ss.clear(); ss.str("");
		switch (c)
		{
		case 'l':
			ss.str(optarg);
			ss >> led_count;
			if (led_count <= 0 || led_count > 1024) {
				std::cerr << "ERROR: LED count is out of range: " << led_count << std::endl;
				return 1;
			}
			break;
		case 'c':
			ParseCalib(calib, optarg);
			break;
		case 'h':
			PrintUsage();
			return 0;
		case '?':
			if (isprint(optopt))
				std::cerr << "Unknown option `-" << (char)optopt << "'." << std::endl;
			return 1;
		}
	}

	try
	{
		ite.Init();
	}
	catch (std::runtime_error & ex)
	{
		std::cerr << ex.what() << std::endl;
		return 1;
	}

#if _WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
		std::cerr << "\nERROR: Could not set control handler\n" << std::endl;
		return 1;
	}

	MyWindow win(100, 100);
	win.AddSuspendCB([&ite]() {
		Suspend(ite);
	});
	win.AddResumeCB(
		[&ite]() {
		Resume(ite);
	});

#else
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	#if defined(HAVE_DBUS)
	dbusmgr::dbus_manager dmgr;
	#endif

#endif

	std::cout << "LED count per strip: " << led_count << std::endl;
	ite.SetLedCount(LedCountToEnum(led_count));

	//optreset = 1; // should be but is undefined, dafuq
	optind = 1;
	while ((c = getopt(argc, argv, getopt_args)) != -1)
	{
		ss.clear(); ss.str("");
		switch (c)
		{
		case 'c':
		case 'l':
			break;
		case 'a':
			ParseSetAllPorts(ite, optarg);
			break;
		case 'e':
			ParseEffect(ite, optarg);
			break;
		case 'h':
			PrintUsage();
			return 0;
		case 'r':
			if (led_count <= 0) {
				std::cerr << "ERROR: Specify led count with -l" << std::endl;
				return 1;
			}

#if defined(HAVE_DBUS)
			try
			{
				dmgr.init([&ite](bool suspending) {
					if (suspending) {
						Suspend(ite);
					} else {
						Resume(ite);
					}
				});
			} catch (std::runtime_error& err) {
				std::cerr << err.what() << std::endl;
			}
#endif

			ite.DisableEffect(true);
			std::cout << "CTRL + C to stop RGB loop" << std::endl;
			DoRainbow(ite, led_count, calib);
			ite.DisableEffect(false);
			break;
		case 's':
			std::cout << "Stopping all" << std::endl;
			ite.StopAll();
			break;
		case '?':
			if (optopt == 'l' || optopt == 'a' || optopt == 'e')
				std::cerr << "ERROR: Option -" << (char)optopt << " requires an argument." << std::endl;
			//else if (isprint(optopt))
			//	std::cerr << "Unknown option `-" << (char)optopt << "'." << std::endl;
			//else
			//	std::cerr <<
			//		"Unknown option character `\\x"
			//		<< std::hex << (int)optopt
			//		<< "'." << std::endl;
			return 1;
		default:
			abort();
		}
	}

	return 0;
}

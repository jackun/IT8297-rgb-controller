#include <emmintrin.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <array>
#include <cstring>
#include <cmath>

#include <rgblights.h>

#if _WIN32
#include "wgetopt.h"
#else
#include <unistd.h>
//#include <getopt.h>
#endif

using namespace rgblights;
using ms = std::chrono::milliseconds;
using clk = std::chrono::high_resolution_clock;

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

struct Color
{
	float r;
	float g;
	float b;
};

/*! \brief Convert HSV to RGB color space

  Converts a given set of HSV values `h', `s', `v' into RGB
  coordinates. The output RGB values are in the range [0, 1], and
  the input HSV values are in the ranges h = [0, 360], and s, v =
  [0, 1], respectively.

  \param c  Red,Green,Blue components, used as output, range: [0, 1]
  \param fH Hue component, used as input, range: [0, 360]
  \param fS Hue component, used as input, range: [0, 1]
  \param fV Hue component, used as input, range: [0, 1]

*/
void HSVtoRGB(Color& c, float fH, float fS, float fV) {
	float fC = fV * fS; // Chroma
	float fHPrime = fmod(fH / 60.0, 6);
	float fX = fC * (1 - fabs(fmod(fHPrime, 2) - 1));
	float fM = fV - fC;

	if (0 <= fHPrime && fHPrime < 1) {
		c.r = fC;
		c.g = fX;
		c.b = 0;
	}
	else if (1 <= fHPrime && fHPrime < 2) {
		c.r = fX;
		c.g = fC;
		c.b = 0;
	}
	else if (2 <= fHPrime && fHPrime < 3) {
		c.r = 0;
		c.g = fC;
		c.b = fX;
	}
	else if (3 <= fHPrime && fHPrime < 4) {
		c.r = 0;
		c.g = fX;
		c.b = fC;
	}
	else if (4 <= fHPrime && fHPrime < 5) {
		c.r = fX;
		c.g = 0;
		c.b = fC;
	}
	else if (5 <= fHPrime && fHPrime < 6) {
		c.r = fC;
		c.g = 0;
		c.b = fX;
	}
	else {
		c.r = 0;
		c.g = 0;
		c.b = 0;
	}

	c.r += fM;
	c.g += fM;
	c.b += fM;
}

Color BlendSqrt(Color a, Color b, float factor)
{
	Color c;
	float inv = 1.f - factor;

	if (factor < 1.0e-6 /*FLT_EPSILON*/) return a;
	c.r = std::min(std::sqrt(a.r * a.r * inv + b.r * b.r * factor), 1.f);
	c.g = std::min(std::sqrt(a.g * a.g * inv + b.g * b.g * factor), 1.f);
	c.b = std::min(std::sqrt(a.b * a.b * inv + b.b * b.b * factor), 1.f);

	return c;
}

Color Blend(Color a, Color b, float factor)
{
	Color c;
	float inv = 1.f - factor;

	if (factor < 1.0e-6 /*FLT_EPSILON*/) return a;
	c.r = std::min(a.r * inv + b.r * factor, 1.f);
	c.g = std::min(a.g * inv + b.g * factor, 1.f);
	c.b = std::min(a.b * inv + b.b * factor, 1.f);

	return c;
}

void DoEdgeBlender(UsbIT8297& usbDevice, uint32_t led_count, LEDs& calib)
{
	int repeat_count = 1;
	int delay_ms = 16; // 16ms - limit refresh to ~60fps, seems max is about 100 leds (with libusb atleast)
	float edge0_hue = 0, edge1_hue = 0;
	Color edge0, edge1;
	Color c;

	float hue_step = 1;
	float hue_stretch = .5f;
	float hue_offset = 0;
	bool dir = true;
	float pulse = 0.1f;
	float pulse_min = 0.1f;
	float pulse_speed = 0.01f;

	std::vector<uint32_t> led_data(led_count);

	while (running)
	{
		if (pause_loop) {
			std::this_thread::sleep_for(ms(100));
			continue;
		}

		auto curr = clk::now();

		edge0_hue = (hue_offset);
		edge1_hue = (361.f - hue_offset);

		HSVtoRGB(edge0, edge0_hue, 1.f, 1.f);
		HSVtoRGB(edge1, edge1_hue, 1.f, 1.f);

		for (size_t i = 0; i < led_data.size(); i++)
		{
			float factor = float(i) / led_data.size();
			c = Blend(edge0, edge1, factor);
			led_data[i] = (uint32_t)(c.r * pulse * calib.r)
						| (uint32_t)(c.g * pulse * calib.g) << 8
						| (uint32_t)(c.b * pulse * calib.b) << 16;
		}

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

		hue_offset = fmodf(hue_offset + hue_step, 361.f);

		auto dur = std::chrono::duration_cast<ms>(clk::now() - curr).count();
		std::this_thread::sleep_for(ms(std::max<int64_t>(delay_ms - dur, 0)));
	}
}

void DoRainbow(UsbIT8297& usbDevice, uint32_t led_count, LEDs& calib)
{
	int repeat_count = 1;
	int delay_ms = 16; // 16ms - limit refresh to ~60fps, seems max is about 100 leds (with libusb atleast)
	float hue = 0;
	float hue_step = 1;
	float hue_stretch = .5f;
	float hue_offset = 0;
	Color c;
	bool dir = true;
	float pulse = 0.1f;
	float pulse_min = 0.1f;
	float pulse_speed = 0.01f;

	std::vector<uint32_t> led_data(led_count);

	while (running)
	{
		if (pause_loop) {
			std::this_thread::sleep_for(ms(100));
			continue;
		}
		auto curr = clk::now();

		for (size_t i = 0; i < led_data.size(); i++)
		{
			// FIXME ewww
			hue = fmodf((361.f - hue_offset) + (361.f / led_data.size()) * i * hue_stretch, 361.f);

			HSVtoRGB(c, hue, 1.f, 1.f);
			led_data[i] = (uint32_t)(c.r * pulse * calib.r)
				| (uint32_t)(c.g * pulse * calib.g) << 8
				| (uint32_t)(c.b * pulse * calib.b) << 16;
		}

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

		hue_offset = fmodf(hue_offset + hue_step, 361);

		auto dur = std::chrono::duration_cast<ms>(clk::now() - curr).count();
		std::this_thread::sleep_for(ms(std::max<int64_t>(delay_ms - dur, 0)));
	}
}

void snake_fadeout(std::vector<uint32_t>& led_data, uint8_t step)
{
	const __m128i step128 = _mm_setr_epi8(step, step, step, 0, step, step, step, 0, step, step, step, 0, step, step, step, 0);
	// snake effect, substract step from individual RGB color bytes
	size_t led_data_size = led_data.size();
	led_data_size -= led_data_size % 4;

	int i;
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
}

// FIXME only works for up to 127 leds (half of uint8_t)
void DoSnake(UsbIT8297& usbDevice, uint32_t led_count, LEDs& calib)
{
	int delay_ms = (int)(32 * (60.f / led_count)); // 60 leds' speed as base line
	size_t led_offset = 0;
	size_t color_offset = 0;

	if (led_count < 2 || led_count > 127) {
		std::cerr << "ERROR: Too few or many leds. Only works with 2 to 127 leds." << std::endl;
		return;
	}

	//defaults to 32 leds usually
	std::vector<uint32_t> led_data(led_count);

	float divider = 2.f; // decrease for wider unlit cap and vice versa
	const uint8_t step = (uint8_t)(255.f / led_data.size() / divider); /* roughly amount of lit leds */

	std::cout << "Fade step:" << (int)step << std::endl;

	while (running)
	{
		if (pause_loop) {
			std::this_thread::sleep_for(ms(1500));
			continue;
		}

		//static const uint32_t colors[] = { 0x00FF0000, 0x0000FF00, 0x000000FF };
		static const uint32_t colors[] = { 0x00643264, 0x00643232, 0x00646432, 0x00326432, 0x00326464, 0x00323264 };

		for (int j = 0; j < led_data.size(); j++)
		{
			if (!running)
				break;

			auto curr = clk::now();

			snake_fadeout(led_data, step);

			led_data[led_offset] = colors[(led_offset % countof(colors))];
			//led_data[led_offset] = colors[color_offset];

			if (!usbDevice.SendRGB(led_data))
				return;

			led_offset = (led_offset + 1) % led_data.size();

			auto dur = std::chrono::duration_cast<ms>(clk::now() - curr).count();
			std::this_thread::sleep_for(ms(std::max<int64_t>(delay_ms - dur, 0)));
		}
		color_offset = (color_offset + 1) % countof(colors);
	}
}

void DoSnakeRainbow(UsbIT8297& usbDevice, uint32_t led_count, LEDs& calib)
{
	Color c;
	int delay_ms = (int)(32 * (60.f / led_count)); // 60 leds' speed as base line
	size_t led_offset = 0;
	float hue = 0;

	if (led_count < 2 || led_count > 127) {
		std::cerr << "ERROR: Too few or many leds. Only works with 2 to 127 leds." << std::endl;
		return;
	}

	//defaults to 32 leds usually
	std::vector<uint32_t> led_data(led_count);

	float divider = .75f; // decrease for wider unlit cap and vice versa
	const uint8_t step = (uint8_t)(255.f / led_data.size() / divider); /* roughly amount of lit leds */

	std::cout << "Fade step:" << (int)step << std::endl;

	while (running)
	{
		if (pause_loop) {
			std::this_thread::sleep_for(ms(1500));
			continue;
		}

		for (int j = 0; j < led_data.size(); j++)
		{
			if (!running)
				break;

			auto curr = clk::now();

			snake_fadeout(led_data, step);

			HSVtoRGB(c, hue, 1.f, 1.f);
			led_data[led_offset] = (uint32_t)(c.r * calib.r)
				| (uint32_t)(c.g * calib.g) << 8
				| (uint32_t)(c.b * calib.b) << 16;

			if (!usbDevice.SendRGB(led_data))
				return;

			led_offset = (led_offset + 1) % led_data.size();
			auto dur = std::chrono::duration_cast<ms>(clk::now() - curr).count();
			std::this_thread::sleep_for(ms(std::max<int64_t>(delay_ms - dur, 0)));

			hue = hue + (360.f / 8.f) / led_data.size();
			if (hue >= 360.f)
				hue = 0;
		}
	}
}

void ParseSetAllPorts(UsbIT8297Base& ite, const char * const opt)
{
	PktEffect effect;
	std::vector<uint32_t> params {1, 0, 7, 1, 1200, 1200, 200}; //defaults
	std::stringstream ss(opt);

	std::string token;
	int i = 0;
	while (std::getline(ss, token, ',')) {
		if (i==1) {
			uint8_t r,g,b;
			if (sscanf(token.c_str(), "%02hhX%02hhX%02hhX,", &r, &g, &b) == 3) {
				std::cerr << (int)r << ","<< (int)g << ","<< (int)b << std::endl;
				params[i] = (r << 16 | g << 8 | b);
			} else {
				std::cerr << "Failed to parse color" << std::endl;
			}
		} else if (i < params.size())
			params[i] = std::stoi(token);
		i++;
	}

	if (params.size() < 2 || params.size() > 7) {
		std::cerr << "Failed to parse argument list." << std::endl;
		return;
	}

	ite.SetAllPorts(static_cast<EffectType>(params[0]), 
		params[1], //color
		params[2], //param0
		params[3], //param2
		params[4], //period0
		params[5], //period1
		params[6]); //period2
}

void ParseEffect(UsbIT8297Base& ite, const char * const opt)
{
	PktEffect effect;
	std::vector<uint32_t> params;
	std::stringstream ss(opt);

	std::string token;
	int i = 0;
	while (std::getline(ss, token, ',')) {
		if (i == 4 || i == 5) {
			uint8_t r,g,b;
			if (sscanf(token.c_str(), "%02hhX%02hhX%02hhX,", &r, &g, &b) == 3) {
				params.push_back(r << 16 | g << 8 | b);
			} else {
				std::cerr << "Failed to parse color" << (i - 4) << ". Defaulting to #FF2100" << std::endl;
				params.push_back(0xFF2100);
			}
		}
		else
			params.push_back(std::stoi(token));
		i++;
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
	"    Arguments are parsed and run in sequence.\n"
	"    E.g. use '-l 120 -a 2,ff2100 -r 0 -s' to set all ports to pulsing orange, run custom RGB effect over 120 LEDs and then stop all effects after quiting.\n\n"
	"    -a EFFECT,COLOR[,param0[,param2[,period0,period1,period2]]]\n"
	"        set all ports to effect, color in hexadecimal\n\n"

	"    -c R,G,B\n"
	"        LED calibration, value range is 0..255. Normalizes custom effect to given range (basically max brightness of given color)\n\n"

	"    -e HEADER,EFFECT[,other,params]\n"
	"        set built-in effect and its parameters (comma separated arguments), where:\n"
	"    \tHEADER\n"
	"    \t  32..39 (may depend on actual hardware)\n"
	"    \tEFFECT\n"
	"    \t  1 - static\n"
	"    \t  2 - pulse\n"
	"    \t  3 - flash\n"
	"    \t  4 - colorcycle\n"
	"    \t >4 - might have some more built-in effects\n"
	"    \tMAX BRIGHTNESS\t (default 100)\n"
	"    \tMIN BRIGHTNESS\t (default 0)\n"

	"    \tCOLOR 0    - main effect color (in hexadecimal format)\n"
	"    \tCOLOR 1\n"
	"    \tPERIOD 0   - ex fade in speed (default 1200)\n"
	"    \tPERIOD 1   - ex fade out speed (default 1200)\n"
	"    \tPERIOD 2   - ex hold period (default 200)\n"
	"    \tPERIOD 3   - (default 0)\n\n"

	"    \tEFFECT PARAM 0\t- e.g. pulse/flash/colorcycle color cycle count (max 7)\n"
	"    \tEFFECT PARAM 1\n"
	"    \tEFFECT PARAM 2\t- e.g. flash repeat count\n"
	"    \tEFFECT PARAM 3\n\n"

	"    -l COUNT \t- LED count per strip\n"
	"    -r PRESET\t- custom effect (needs preciding -l)\n"
	"        1 - rainbow\n"
	"        2 - edge blend\n"
	"        3 - snake\n"
	"        4 - rainbow snake\n"
	"    -s         \t- stop all effects\n"
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

std::array<decltype(&DoRainbow), 4> effects = {
	DoRainbow,
	DoEdgeBlender,
	DoSnake,
	DoSnakeRainbow,
};

int main(int argc, char* const * argv)
{
	PktEffect effect;
	UsbIT8297 ite;
	uint32_t led_count = 32;
	uint32_t custom_effect = 0;
	const char * getopt_args = "a:c:r:shl:e:";
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
			//if (isprint(optopt))
			//	std::cerr << "Unknown option `-" << (char)optopt << "'." << std::endl;
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
		//return 1;
	}

#if _WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
		std::cerr << "\nERROR: Could not set control handler\n" << std::endl;
		return 1;
	}

	MyWindow win(100, 100);
	win.AddCallback([&ite](bool suspending) {
		if (suspending)
			Suspend(ite);
		else
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
			try {
				ParseSetAllPorts(ite, optarg);
			} catch(std::invalid_argument& e) {
				std::cerr << "SetAllPorts: invalid argument: " << e.what() << std::endl;
			} catch(std::exception& e) {
				std::cerr << "SetAllPorts: ERROR:" << e.what() << std::endl;
			}
			break;
		case 'e':
			try {
				ParseEffect(ite, optarg);
			} catch(std::invalid_argument& e) {
				std::cerr << "Effect: invalid argument: " << e.what() << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Effect: ERROR:" << e.what() << std::endl;
			}
			break;
		case 'h':
			PrintUsage();
			return 0;
		case 'r':
			if (led_count <= 0) {
				std::cerr << "ERROR: Specify led count with -l" << std::endl;
				return 1;
			}

			ss.str(optarg);
			ss >> custom_effect;
			custom_effect--;
			if (custom_effect > effects.size()) {
				std::cerr << "ERROR: custom effect is out of range: " << custom_effect << " of " << effects.size() << std::endl;
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
			effects[custom_effect](ite, led_count, calib);
			ite.DisableEffect(false);
			break;
		case 's':
			std::cout << "Stopping all" << std::endl;
			ite.StopAll();
			break;
		case '?':
			if (optopt == 'l' || optopt == 'a' || optopt == 'e' || optopt == 'r')
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
			break;
		}
	}

	return 0;
}

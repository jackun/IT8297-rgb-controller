/*
 * SoundManagerBase.cpp
 *
 *	Created on: 11.12.2011
 *		Project: Lightpack
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack a USB content-driving ambient lighting system
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <iostream>
#include <iomanip>
#include "SoundManagerBase.hpp"
#include "onsetsds/onsetsds.h"

/*
#if defined(Q_OS_MACOS)
#include "MacOSSoundManager.h"
#elif defined(Q_OS_WIN) && defined(BASS_SOUND_SUPPORT)
#include "WindowsSoundManager.hpp"
#elif defined(Q_OS_LINUX) && defined(PULSEAUDIO_SUPPORT)
#include "PulseAudioSoundManager.hpp"
#endif
*/

#include "PulseAudioSoundManager.hpp"

static std::unique_ptr<OnsetsDS> ods;
static std::vector<float> ods_buff;

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//		if s == 0, then h = -1 (undefined)

static void RGBtoHSV( float r, float g, float b, float& h, float& s, float& v )
{
	float min, max, delta;

	min = std::min( r, std::min(g, b ));
	max = std::max( r, std::max(g, b ));
	v = max;				// v

	delta = max - min;

	if( max != 0 )
		s = delta / max;		// s
	else {
		// r = g = b = 0		// s = 0, v is undefined
		s = 0;
		h = -1;
		return;
	}

	if( r == max )
		h = ( g - b ) / delta;		// between yellow & magenta
	else if( g == max )
		h = 2 + ( b - r ) / delta;	// between cyan & yellow
	else
		h = 4 + ( r - g ) / delta;	// between magenta & cyan

	h *= 60;				// degrees
	if( h < 0 )
		h += 360;

}

static void HSVtoRGB( float& r, float& g, float& b, float h, float s, float v )
{
	int i;
	float f, p, q, t;

	if( s == 0 ) {
		// achromatic (grey)
		r = g = b = v;
		return;
	}

	h /= 60;			// sector 0 to 5
	i = floor( h );
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i ) {
		case 0:
			r = v;
			g = t;
			b = p;
			break;
		case 1:
			r = q;
			g = v;
			b = p;
			break;
		case 2:
			r = p;
			g = v;
			b = t;
			break;
		case 3:
			r = p;
			g = q;
			b = v;
			break;
		case 4:
			r = t;
			g = p;
			b = v;
			break;
		default:		// case 5:
			r = v;
			g = p;
			b = q;
			break;
	}
}

#define GetR(c) (c&0xFF)
#define GetG(c) ((c>>8)&0xFF)
#define GetB(c) ((c>>16)&0xFF)

static void getHsl(uint32_t color, float& hue, float& sat, float& lum)
{
	float r,g,b;
	r = GetR(color) / 255.f;
	g = GetG(color) / 255.f;
	b = GetB(color) / 255.f;
	RGBtoHSV(r, g, b, hue, sat, lum);
}

static void setHsl(uint32_t&color, float hue, float sat, float lum)
{
	float r,g,b;
	HSVtoRGB(r,g,b, hue, sat, lum);
	color = ((uint32_t)(r * 255)) | ((uint32_t)(g * 255) << 8) | ((uint32_t)(b * 255) << 16);
}

void SoundVisualizerBase::interpolateColor(uint32_t& outColor, uint32_t from, uint32_t to, const double value, const double maxValue) {
	float h0, s0, l0;
	float h1, s1, l1;

	getHsl(from, h0, s0, l0);
	getHsl(to, h1, s1, l1);

	if (h0 - h1 > 180)
		h1 += 360;
	else if (h1 - h0 > 180)
		h0 += 360;

	h0 += (h1 - h0) * value / maxValue;
	s0 += (s1 - s0) * value / maxValue;
	l0 += (l1 - l0) * value / maxValue;
	setHsl(outColor, h0, s0, l0);
//	std::cerr << std::hex << std::setfill('0') << std::setw(8) << outColor << "\n";
}

float linear(float a, float b, float t)
{
    return a * (1 - t) + b * t;
}

template<typename F>
void interpolate(uint32_t& outColor, uint32_t a, uint32_t b, float t, F interpolator)
{
    // 0.0 <= t <= 1.0
	float r0, g0, b0;
	float r1, g1, b1;

	r0 = GetR(a);
	g0 = GetG(a);
	b0 = GetB(a);

	r1 = GetR(b);
	g1 = GetG(b);
	b1 = GetB(b);

    int out_r = (int)std::min(interpolator(r0, r1, t), 255.f);
    int out_g = (int)std::min(interpolator(g0, g1, t), 255.f);
    int out_b = (int)std::min(interpolator(b0, b1, t), 255.f);

    outColor = out_r | out_g << 8 | out_b << 16;
}


bool TwinPeaksSoundVisualizer::visualize(const float* const fftData, const size_t fftSize, std::vector<uint32_t>& colors)
{
	bool changed = false;
	const size_t middleLed = std::floor(colors.size() / 2);
	/*{ // test interpolator
		uint32_t from = m_generator.current();
		uint32_t to = from;
		if (m_isLiquidMode) {
			float h, s, l;
			getHsl(from, h, s, l);
			setHsl(from, h, s, 120.f/255.f);
			getHsl(to, h, s, l);
			setHsl(to, (h + 180), s, 120.f/255.f);
			//from.setHsl(from.hue(), from.saturation(), 120);
			//to.setHsl(to.hue() + 180, to.saturation(), 120);
		}

		uint32_t i = 0;
		for (auto & c : colors)
				//interpolateColor(c, from, to, i++, middleLed);
				interpolate(c, from, to, float(i++) / (middleLed*2), &linear);
		return true;
	}*/

	// most sensitive Hz range for humans
	// this assumes 44100Hz sample rate
	const size_t optimalHzMin = 1950 / (44100 / 2 / fftSize); // 2kHz
	const size_t optimalHzMax = 5050 / (44100 / 2 / fftSize); // 5kHz

	float currentPeak = 0.0f;
	for (size_t i = 0; i < fftSize; ++i) {
		float mag = fftData[i];
		if (i > optimalHzMin && i < optimalHzMax)// amplify sensitive range
			mag *= 6.0f;
		currentPeak += mag;
	}

	if (m_previousPeak < currentPeak)
		m_previousPeak = currentPeak;
	else
		m_previousPeak = std::max(0.0f, m_previousPeak - (fftSize * 0.000005f)); // lower the stale peak so it doesn't persist forever

	const size_t thresholdLed = m_previousPeak != 0.0f ? middleLed * (currentPeak / m_previousPeak) : 0;

	// if leds move a lot with x% amplitude, increase fade speed
	if (std::abs((int)(thresholdLed - m_prevThresholdLed)) > middleLed * 0.2)
		m_speedCoef = std::min(10.0, m_speedCoef + 2.0);
	else // reset on slow down
		m_speedCoef = 1.0;// std::max(1.0, speedCoef - 2.0);

	for (size_t idxA = 0; idxA < middleLed; ++idxA) {
		const size_t idxB = colors.size() - 1 - idxA;
		uint32_t color = 0;
		if (idxA < thresholdLed) {
			uint32_t from = m_isLiquidMode ? m_generator.current() : m_minColor;
			uint32_t to = m_isLiquidMode ? from : m_maxColor;
			if (m_isLiquidMode) {
				float h, s, l;
				getHsl(from, h, s, l);
				setHsl(from, h, s, 120.f/255.f);
				getHsl(to, h, s, l);
				setHsl(to, (h + 180), s, 120.f/255.f);
				//from.setHsl(from.hue(), from.saturation(), 120);
				//to.setHsl(to.hue() + 180, to.saturation(), 120);
			}
			//interpolateColor(color, from, to, idxA, middleLed);
			interpolate(color, from, to, float(idxA) / middleLed, &linear);
		}
		else if (FadeOutSpeed > 0 && (colors[idxA] > 0 || colors[idxB] > 0)) { // fade out old peaks
			float h,s,l;
			uint32_t oldColor(std::max(colors[idxA], colors[idxB])); // both colors are either the same or one is 0, so max() is good enough here
			getHsl(oldColor, h, s, l);
			float new_l = l * 255 - FadeOutSpeed * ((thresholdLed > 0 ? thresholdLed : 1) / (double)idxA)* m_speedCoef;
			const int luminosity = std::max(std::min(new_l, 255.0f), 0.0f);
			setHsl(oldColor, h, s, luminosity / 255.f);
			color = oldColor;
		}

		// peak A
		uint32_t colorA = color; //Settings::isLedEnabled(idxA) ? color : 0;
		changed = changed || (colors[idxA] != colorA);
		colors[idxA] = colorA;

		// peak B
		uint32_t colorB = color; //Settings::isLedEnabled(idxB) ? color : 0;
		changed = changed || (colors[idxB] != colorB);
		colors[idxB] = colorB;
	}
	if (currentPeak == 0.0f) // reset peak on silence
		m_previousPeak = currentPeak;

	m_prevThresholdLed = thresholdLed;
	return changed;
}



SoundManagerBase* SoundManagerBase::create(int hWnd)
{
#if defined(Q_OS_MACOS)
	Q_UNUSED(hWnd);
	return new MacOSSoundManager();
#elif defined(Q_OS_WIN) && defined(BASS_SOUND_SUPPORT)
	return new WindowsSoundManager(hWnd);
#elif defined(Q_OS_LINUX) && defined(PULSEAUDIO_SUPPORT)
	Q_UNUSED(hWnd);
	return new PulseAudioSoundManager();
#endif

	return new PulseAudioSoundManager();
	return nullptr;
}

SoundManagerBase::SoundManagerBase()
{
	m_fft = (float *)calloc(fftSize(), sizeof(*m_fft));
}

SoundManagerBase::~SoundManagerBase()
{
	if (m_isEnabled) start(false);
	if (m_fft)
		free((void *)m_fft);
	if (m_visualizer)
		delete m_visualizer;
	ods = nullptr;
}

size_t SoundManagerBase::fftSize() const
{
	const size_t size = 1024;
	static_assert(size && !(size & (size - 1)), "FFT size has to be a power of 2");
	return size;
}

float* SoundManagerBase::fft() const
{
	return m_fft;
}

void SoundManagerBase::setDevice(int value)
{

	bool enabled = m_isEnabled;
	if (enabled) start(false);
	m_device = value;
	if (enabled) start(true);
}

void SoundManagerBase::setMinColor(rgblights::LEDs color)
{
	if (m_visualizer)
		m_visualizer->setMinColor(color);
}

void SoundManagerBase::setMaxColor(rgblights::LEDs color)
{
	if (m_visualizer)
		m_visualizer->setMaxColor(color);
}

void SoundManagerBase::setNumberOfLeds(int numberOfLeds)
{
	initColors(numberOfLeds);
}

void SoundManagerBase::reset()
{
	initColors(m_colors.size());
	if (m_visualizer)
		m_visualizer->reset();
}

void SoundManagerBase::updateColors()
{

	updateFft();
	/*
	bool colorsChanged = (m_visualizer ? m_visualizer->visualize(m_fft, fftSize(), m_colors) : false);
	if (colorsChanged || !m_isSendDataOnlyIfColorsChanged) {
		emit updateLedsColors(m_colors);
		if (m_elapsedTimer.hasExpired(1000)) { // 1s
			emit visualizerFrametime(m_elapsedTimer.restart() / m_frames);
			m_frames = 0;
		}
		m_frames++;
	}*/
}

void SoundManagerBase::initColors(int numberOfLeds)
{
	m_colors.clear();
	if (m_visualizer)
		m_visualizer->clear(numberOfLeds);

	for (int i = 0; i < numberOfLeds; i++)
		m_colors.push_back({});
}

void SoundManagerBase::setVisualizer(int value)
{
	bool running = false;
	if (m_visualizer) {
		running = m_visualizer->isRunning();
		delete m_visualizer;
		m_visualizer = nullptr;
	}
/*	m_visualizer = SoundVisualizerBase::createWithID(value);
	if (m_visualizer) {
		m_visualizer->setMinColor(Settings::getSoundVisualizerMinColor());
		m_visualizer->setMaxColor(Settings::getSoundVisualizerMaxColor());
		m_visualizer->setLiquidMode(Settings::isSoundVisualizerLiquidMode());
		m_visualizer->setSpeed(Settings::getSoundVisualizerLiquidSpeed());
		m_visualizer->clear(m_colors.size());
		if (running || m_isEnabled)
			m_visualizer->start();
	}*/
}

void SoundManagerBase::updateOnset()
{
	if (!ods)
	{
		auto odftype = ODS_ODF_COMPLEX;
		ods = std::make_unique<OnsetsDS>();
		ods_buff.resize(onsetsds_memneeded(odftype, fftSize(), 11));
		onsetsds_init(ods.get(), ods_buff.data(), ODS_FFT_FFTW3_HC, odftype, fftSize(), 11, 44100);
	}

	if (onsetsds_process(ods.get(), fft()))
		m_onsets++;
}


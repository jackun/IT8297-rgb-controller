/*
 * SoundManagerBase.hpp
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

#pragma once

//#include "SoundVisualizer.hpp"
#include <vector>
#include <string>
#include <rgblights.h>
#include "LiquidColorGenerator.hpp"

struct SoundVisualizerBase
{
	virtual bool isRunning() = 0;
	virtual void setMinColor(rgblights::LEDs color) = 0;
	virtual void setMaxColor(rgblights::LEDs color) = 0;
	virtual void reset() = 0;
	void start() {
		m_generator.start();
	}
	virtual void stop() = 0;
	virtual void clear(int count) = 0;
	virtual bool visualize(const float* const fftData, const size_t fftSize, std::vector<uint32_t>& colors) = 0;

	void interpolateColor(uint32_t& outColor, uint32_t from, uint32_t to, const double value, const double maxValue);
	void setSpeed(int speed)
	{
		m_generator.setSpeed(speed);
	}

	void updateColor()
	{
		m_generator.updateColor();
	}

protected:
	uint32_t 	m_minColor;
	uint32_t 	m_maxColor;
	LiquidColorGenerator m_generator;
	bool	m_isLiquidMode{ true };
	bool	m_isRunning{ false };
	size_t  m_frames{ 0 };
};

struct TwinPeaksSoundVisualizer : public SoundVisualizerBase
{
	bool isRunning() { return true; }
	void setMinColor(rgblights::LEDs color) {}
	void setMaxColor(rgblights::LEDs color) {}
	void reset() {}
	void start() {}
	void stop() {}
	void clear(int count) {}
	bool visualize(const float* const fftData, const size_t fftSize, std::vector<uint32_t>& colors);

private:
	float m_previousPeak{ 0.0f };
	size_t m_prevThresholdLed{ 0 };
	double m_speedCoef = 1.0;
	const uint8_t FadeOutSpeed = 12;
};


struct SoundManagerDeviceInfo {
	SoundManagerDeviceInfo(){ this->name = ""; this->id = -1; }
	SoundManagerDeviceInfo(std::string name, int id){ this->name = name; this->id = id; }
	std::string name;
	int id;
};

class SoundManagerBase
{

public:
	SoundManagerBase();
	virtual ~SoundManagerBase();
	static SoundManagerBase* create(int hWnd = 0);

	void updateLedsColors(const std::vector<rgblights::LEDs> & colors);
	void deviceList(const std::vector<SoundManagerDeviceInfo> & devices, int recommended);
	void visualizerFrametime(const double);

public:
	virtual void start(bool isEnabled) { };

	// Common options
	void reset();
	virtual size_t fftSize() const;
	float* fft() const;


	void setNumberOfLeds(int value);
	void setDevice(int value);
	void setVisualizer(int value);
	void setMinColor(rgblights::LEDs color);
	void setMaxColor(rgblights::LEDs color);
	void updateColors();

protected:
	virtual bool init() = 0;
	void initColors(int numberOfLeds);
//	virtual void populateDeviceList(QList<SoundManagerDeviceInfo>& devices, int& recommended) = 0;
	virtual void updateFft() {};

protected:
	SoundVisualizerBase* m_visualizer{nullptr};

	std::vector<rgblights::LEDs> m_colors;

	bool	m_isEnabled{false};
	bool	m_isInited{false};
	int		m_device{-1};
	bool	m_isSendDataOnlyIfColorsChanged{false};
	
	float*	m_fft{nullptr};
	
	//QElapsedTimer m_elapsedTimer;
	size_t m_frames{ 1 };
};

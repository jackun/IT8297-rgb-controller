/*
 * MoodLampManager.hpp
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
#include <vector>
#include <tuple>

class LiquidColorGenerator
{
public:
	explicit LiquidColorGenerator();

	//void updateColor(uint32_t color);

public:
	void start();
	void stop();
	//std::tuple<uint32_t, uint32_t, uint32_t> current();
	uint32_t current();
	void reset();

public:
	void setSpeed(int value);
	void updateColor();

private:
	int generateDelay();
	uint32_t generateColor();

private:
	bool m_isEnabled;

	int m_delay;
	int m_speed = 5;

	int m_red = 0;
	int m_green = 0;
	int m_blue = 0;

	int m_redNew = 0;
	int m_greenNew = 0;
	int m_blueNew = 0;

	static const int ColorsMoodLampCount = 14;
	static const uint32_t AvailableColors[ColorsMoodLampCount];
	std::vector<uint32_t> m_unselectedColors;

};

/*
 * MoodLampManager.cpp
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

#include "LiquidColorGenerator.hpp"
#include <iostream>
#include <cstdlib>     /* srand, rand */
#include <ctime>       /* time */

const uint32_t LiquidColorGenerator::AvailableColors[LiquidColorGenerator::ColorsMoodLampCount] =
{
	0xFFFFFF, //Qt::white,
	0x0000FF, //Qt::red,
	0x00FFFF, //Qt::yellow,
	0x00FF00, //Qt::green,
	0xFF0000, //Qt::blue,
	0xFF00FF, //Qt::magenta,
	0xFFFF00, //Qt::cyan,
	0x000080, //Qt::darkRed,
	0x008000, //Qt::darkGreen,
	0x800000, //Qt::darkBlue,
	0x008080, //Qt::darkYellow,
	0x0080FF, //qRgb(255,128,0),
	0x80FFFF, //qRgb(128,255,255),
	0x8000FF, //qRgb(128,0,255)
};

LiquidColorGenerator::LiquidColorGenerator()
{

	srand(time(nullptr));

	m_isEnabled = false;
	//m_timer.setTimerType(Qt::PreciseTimer);
	//connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateColor()));
}

void LiquidColorGenerator::start()
{

	m_isEnabled = true;

	reset();
	updateColor();
}

void LiquidColorGenerator::stop()
{

	m_isEnabled = false;

//	m_timer.stop();
}

//std::tuple<uint32_t, uint32_t, uint32_t>
uint32_t LiquidColorGenerator::current()
{
	//return {(uint32_t)m_red&0xFF, (uint32_t)m_green&0xFF, (uint32_t)m_blue&0xFF};
	uint32_t col = m_red | (m_green << 8) | (m_blue << 16);
	return col;
}

void LiquidColorGenerator::setSpeed(int value)
{
	m_speed = value;
	m_delay = generateDelay();
}

void LiquidColorGenerator::reset()
{
	m_red = 0;
	m_green = 0;
	m_blue = 0;
	m_unselectedColors.clear();
}

void LiquidColorGenerator::updateColor()
{

	if (m_red == m_redNew && m_green == m_greenNew && m_blue == m_blueNew)
	{
		m_delay = generateDelay();
		uint32_t colorNew = generateColor();

		m_redNew = colorNew & 0xFF;
		m_greenNew = (colorNew & 0xFF00) >> 8;
		m_blueNew = (colorNew & 0xFF0000) >> 16;

	}

	if (m_redNew != m_red)	{ if (m_red	> m_redNew)	--m_red;	else ++m_red; }
	if (m_greenNew != m_green){ if (m_green > m_greenNew) --m_green; else ++m_green; }
	if (m_blueNew != m_blue) { if (m_blue	> m_blueNew)	--m_blue;	else ++m_blue; }


	//emit updateColor(QColor(m_red, m_green, m_blue));

	if (m_isEnabled)
	{
		//m_timer.start(m_delay);
	}
}

int LiquidColorGenerator::generateDelay()
{
	return 1000 / (m_speed + (rand() % 25) + 1);
}

uint32_t LiquidColorGenerator::generateColor()
{
	if (m_unselectedColors.empty())
	{
		for (int i = 0; i < ColorsMoodLampCount; i++)
			m_unselectedColors.push_back(AvailableColors[i]);
	}

	int randIndex = rand() % std::max(1ul, m_unselectedColors.size());
	std::cerr << "randIndex: " << randIndex << std::endl;

	uint32_t col = m_unselectedColors[randIndex];
	m_unselectedColors.erase(m_unselectedColors.begin() + randIndex);
	return col;
}

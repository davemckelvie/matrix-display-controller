/**
 * LED Matrix library for http://www.seeedstudio.com/depot/ultrathin-16x32-red-led-matrix-panel-p-1582.html
 * The LED Matrix panel has 32x16 pixels. Several panel can be combined together as a large screen.
 *
 * Coordinate & Connection (Arduino -> panel 0 -> panel 1 -> ...)
 *   (0, 0)                                     (0, 0)
 *     +--------+--------+--------+               +--------+--------+
 *     |   5    |    4   |    3   |               |    1   |    0   |
 *     |        |        |        |               |        |        |<----- Arduino
 *     +--------+--------+--------+               +--------+--------+
 *     |   2    |    1   |    0   |                              (64, 16)
 *     |        |        |        |<----- Arduino
 *     +--------+--------+--------+
 *                             (96, 32)
 *  Copyright (c) 2013 Seeed Technology Inc.
 *  @auther     Yihui Xiong
 *  @date       Nov 8, 2013
 *  @license    MIT
 */

#include "LEDMatrix.h"
#include <mbed.h>

// pin to display mapping
#define PIN_A           PC_8
#define PIN_B           PC_7
#define PIN_C           PC_6
#define PIN_D           PB_15
#define PIN_OE          PB_12
#define PIN_STB         PB_13
#define PIN_CLK         PB_14

// colour pins
#define PIN_R1          PB_3
#define PIN_R2          PC_10
#define PIN_G1          PD_2
#define PIN_G2          PC_11
#define PIN_B1          PC_12
#define PIN_B2          PA_15

#if 0
#define ASSERT(e)   if (!(e)) { Serial.println(#e); while (1); }
#else
#define ASSERT(e)
#endif

#define MODULE_HEIGHT	(32)

LEDMatrix::LEDMatrix() : a(PIN_A), b(PIN_B), c(PIN_C), d(PIN_D), r1(PIN_R1), r2(PIN_R2),
                         oe(PIN_OE), stb(PIN_STB), clk(PIN_CLK)
{
    mask = 0xff;
    state = 0;
}

void LEDMatrix::begin(uint8_t *displaybuf, uint16_t width, uint16_t height)
{
    ASSERT(0 == (width % 32));
    ASSERT(0 == (height % 16));

    this->displaybuf = displaybuf;
    this->width = width;
    this->height = height;

    state = 1;
}

void LEDMatrix::drawPoint(uint16_t x, uint16_t y, uint8_t pixel)
{
    ASSERT(width > x);
    ASSERT(height > y);

    uint8_t *byte = displaybuf + x / 8 + y * width / 8;
    uint8_t  bit = x % 8;

    if (pixel) {
        *byte |= 0x80 >> bit;
    } else {
        *byte &= ~(0x80 >> bit);
    }
}

void LEDMatrix::drawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t pixel)
{
    for (uint16_t x = x1; x < x2; x++) {
        for (uint16_t y = y1; y < y2; y++) {
            drawPoint(x, y, pixel);
        }
    }
}

void LEDMatrix::drawImage(uint16_t xoffset, uint16_t yoffset, uint16_t width, uint16_t height, const uint8_t *image)
{
	  for (uint16_t y = 0; y < height; y++) {
		    for (uint16_t x = 0; x < width; x++) {
			      const uint8_t *byte = image + (x + y * width) / width;
            uint8_t  bit = 7 - x % 8;
			      uint8_t  pixel = (*byte >> bit) & 1;

            drawPoint(x + xoffset, y + yoffset, pixel);
        }
    }
}

void LEDMatrix::clear()
{
    uint8_t *ptr = displaybuf;
    for (uint16_t i = 0; i < (width * height / 8); i++) {
        *ptr = 0x00;
        ptr++;
    }
}

void LEDMatrix::reverse()
{
    mask = ~mask;
}

uint8_t LEDMatrix::isReversed()
{
    return mask;
}

void LEDMatrix::scan()
{
    static uint8_t row = 0;  // from 0 to 31

    if (!state) {
        return;
    }

    uint8_t *head = displaybuf + row * (width / 8);
	uint8_t red = row < 16 ? r1 : r2;

    for (uint8_t line = 0; line < (height / MODULE_HEIGHT); line++) {
        uint8_t *ptr = head;
        head += width * 2;              // width * 16 / 8

        for (uint8_t byte = 0; byte < (width / 8); byte++) {
            uint8_t pixels = *ptr;
            ptr++;
            pixels = pixels ^ mask;     // reverse: mask = 0xff, normal: mask =0x00
            for (uint8_t bit = 0; bit < 8; bit++) {
                clk = 0;
                red = pixels & (0x80 >> bit);
                clk = 1;
            }
        }
    }

    oe = 1;              // disable display

    // select row
    a = (row & 0x01);
    b = (row & 0x02);
    c = (row & 0x04);
    d = (row & 0x08);

    // latch data
    stb = 0;
    stb = 1;
    stb = 0;

    oe = 0;              // enable display

    row = (row + 1) & 0x1f;
}

void LEDMatrix::on()
{
    state = 1;
}

void LEDMatrix::off()
{
    state = 0;
    oe = 1;
}

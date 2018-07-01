/*
 * Copyright (C) 2017 David McKelvie.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mbed.h>
#include <string>
#include "LEDMatrix.h"
#include "font.h"

//TODO: HUB75 RGB display
//      - 8 bit RGB with 'pwm'

// bus stop display 3 x 64 x 32 = 192 x 32 = 24 bytes width, 32 height
#define WIDTH   192   // pixels, 24 bytes
#define HEIGHT  32    // pixels
#define CHAR_WIDTH  6 // including 1 pixel space to left
#define CHAR_HEIGHT 8 // including 1 pixel space below
#define DISP_WIDTH (WIDTH / CHAR_WIDTH) // display width in characters
#define STX 2
#define ETX 3
#define BUFF_LEN  200
#define NON_ASCII_LEN 32 // number of ascii control characters available

//#define STM32104
#ifdef STM32104
#define LED_PIN         PC14
// pin to display mapping
#define PIN_A           PA13
#define PIN_B           PA12
#define PIN_C           PA11
#define PIN_D           PA8
#define PIN_OE          PB13
#define PIN_LAT         PB14
#define PIN_CLK         PB15

// colour pins
#define PIN_R1          PB9
#define PIN_R2          PB5
#define PIN_G1          PB8
#define PIN_G2          PB6
#define PIN_B1          PB7
#define PIN_B2          PB4
#else
#define LED_PIN         PC_13

#define MOD_SPI_SLAVE_MOSI PA_7
#define MOD_SPI_SLAVE_MISO PA_6
#define MOD_SPI_SLAVE_SCK PA_5
#define MOD_SPI_SLAVE_SS PA_4

#endif

#define CMD_PRINT_LINE 4
#define CMD_CLEAR_LINE 5
#define CMD_CLEAR_DISP 6
#define CMD_SET_CHARACTER 7
#define CMD_DISPLAY_ON 8
#define CMD_DISPLAY_OFF 9
#define CMD_RGB 10

typedef enum {
  WAIT_FOR_STX,
  GET_COMMAND,
  GET_PARAM,
  GET_DATA,
} processor_state_t;

LEDMatrix matrix;

CircularBuffer<uint8_t, BUFF_LEN> buffer;
DigitalOut ledPin(LED_PIN);
SPISlave spi_slave(MOD_SPI_SLAVE_MOSI, MOD_SPI_SLAVE_MISO, MOD_SPI_SLAVE_SCK, MOD_SPI_SLAVE_SS);

// TODO: RED display has i bit per pixel, RGB needs 24 bits per pixel [R, G, B]
uint8_t displaybuf[WIDTH * HEIGHT / 8] = {0};
uint8_t bufferData[BUFF_LEN] = {0};
uint8_t control[NON_ASCII_LEN][CHAR_HEIGHT] = {0};

static volatile uint8_t command_count = 0;

void overRideControlCharacter(uint8_t index, uint8_t *bitmap)
{
  if (index >= NON_ASCII_LEN) return;
  if (!bitmap) return;

  for (uint8_t i = 0; i < CHAR_HEIGHT; i++) {
    control[index][i] = bitmap[i];
  }
}

void putch(uint8_t x, uint8_t y, char character)
{
  if (character >= 0 && character < 0x20) {
    matrix.drawImage(x, y, CHAR_WIDTH, CHAR_HEIGHT, control[character]);
  } else if (character > 0x1F && character < 0x7F) {
    matrix.drawImage(x, y, CHAR_WIDTH, CHAR_HEIGHT, ASCII[character - 0x20]);
  }
}

void printLine(uint8_t line, string message)
{
  // convert input, line, into x and y
  // line 1: x = 0, y = 0
  // line 2: x = 0, y = 8
  // line 3: x = 0, y = 16
  // line 4: x = 0, y = 24
  uint8_t linePixel = (line - 1) * CHAR_HEIGHT;
  for (uint8_t i = 0; i < message.length(); i++) {
      putch(i * CHAR_WIDTH, linePixel, message.at(i));
  }
}

void printLine(volatile uint8_t *message) {
  uint8_t linePixel = (message[0] - 1) * CHAR_HEIGHT;
  for (int i = 0; i < DISP_WIDTH && message[i + 1]; i++) {
      putch(i * CHAR_WIDTH, linePixel, message[i + 1]);
  }
}

void printLine(uint8_t line, uint8_t *message) {
  uint8_t linePixel = (line - 1) * CHAR_HEIGHT;
  for (int i = 0; i < DISP_WIDTH && message[i]; i++) {
      putch(i * CHAR_WIDTH, linePixel, message[i]);
  }
}

extern "C" {
void __irq_spi1(void);
}

void initSpi()
{
  spi_slave.format(8, 0);
  spi_slave.frequency(4500000);
  NVIC_SetVector(SPI1_IRQn, (uint32_t) __irq_spi1);
  NVIC_SetPriority(SPI1_IRQn, 2);
  NVIC_EnableIRQ(SPI1_IRQn);
  spi_slave.reply((int) 0x20);
}


void __irq_spi1(void)
{
    uint32_t reg = SPI1->DR;
  uint8_t b = (uint8_t) (reg & 0x000000FF);

  if (!buffer.full()) {
    buffer.push(b);
  }

  if (b == ETX) {
    command_count++;
  }
}

void process_character(uint8_t character) {
  static processor_state_t state = WAIT_FOR_STX;
  static uint8_t command = 0;
  static uint8_t param = 0;
  static uint8_t lineBuffer[64];
  static uint8_t index = 0;

  switch (state) {
    case WAIT_FOR_STX:
    if (character == STX) {
      command = param = index = 0;
      state = GET_COMMAND;
    }
    break;

    case GET_COMMAND:
    command = character;
    switch (character) {

      // commands with parameters
      case CMD_PRINT_LINE:
      case CMD_CLEAR_LINE:
      case CMD_SET_CHARACTER:
      state = GET_PARAM;
      break;

      // commands without
      case CMD_CLEAR_DISP:
      matrix.clear();
      state = WAIT_FOR_STX;
      break;

      case CMD_DISPLAY_ON:
      matrix.on();
      state = WAIT_FOR_STX;
      break;

      case CMD_DISPLAY_OFF:
      matrix.off();
      state = WAIT_FOR_STX;
      break;

      default:
      state = WAIT_FOR_STX;
      break;
    }
    break;

    case GET_PARAM:
    param = character;
    switch (command) {
      case CMD_PRINT_LINE:
      case CMD_SET_CHARACTER:
      state = GET_DATA;
      break;

      case CMD_CLEAR_LINE:
      // TODO: clear line
      state = WAIT_FOR_STX;
      break;

      default:
      break;
    }
    break;

    case GET_DATA:
    if (character == ETX) {
      NVIC_DisableIRQ(SPI1_IRQn);
      command_count--;
      NVIC_EnableIRQ(SPI1_IRQn);
      lineBuffer[index] = 0;
      state = WAIT_FOR_STX;
      if (command == CMD_PRINT_LINE) {
        printLine(param, lineBuffer);
      } else if (command == CMD_SET_CHARACTER) {
        overRideControlCharacter(param, lineBuffer);
      }
    } else {
      lineBuffer[index++] = character;
      if (index > 63) {
        state = WAIT_FOR_STX;
      }
    }
    break;

    default:
    state = WAIT_FOR_STX;
    break;
  }
}

void setup()
{
  ledPin = 1;
  initSpi();
  matrix.begin(displaybuf, WIDTH, HEIGHT);
  matrix.reverse();
  printLine(2, "        Where's my bus?");
}

int main()
{
    setup();

    while(1) {
        matrix.scan();

        if (command_count > 0) {
            uint8_t character;
            if (!buffer.empty()) {
              buffer.pop(character);
                process_character(character);
            }
        }
    }

    return 0;
}

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
#include <Arduino.h>
#include <libmaple/dma.h>
#include <SPI.h>
#include <LEDMatrix.h>
#include <font.h>
#include <buffer.h>

// bus stop display 3 x 64 x 32 = 192 x 32 = 24 bytes width, 32 height
#define WIDTH   192   // pixels, 24 bytes
#define HEIGHT  32    // pixels
#define CHAR_WIDTH  6 // including 1 pixel space to left
#define CHAR_HEIGHT 8 // including 1 pixel space below
#define DISP_WIDTH (WIDTH / CHAR_WIDTH) // display width in characters
#define LED_PIN PC13
#define STX 2
#define ETX 3
#define BUFF_LEN  200
#define NON_ASCII_LEN 32 // number of ascii control characters available

#define CMD_PRINT_LINE 4
#define CMD_CLEAR_LINE 5
#define CMD_CLEAR_DISP 6
#define CMD_SET_CHARACTER 7
#define CMD_DISPLAY_ON 8
#define CMD_DISPLAY_OFF 9

typedef enum {
  WAIT_FOR_STX,
  GET_COMMAND,
  GET_PARAM,
  GET_DATA,
} processor_state_t;

LEDMatrix matrix(
  /* A */ PA0,
  /* B */ PA1,
  /* C */ PA2,
  /* D */ PA3,
  /* OE*/ PB4,
  /* R1*/ PB5,
  /* R2*/ PB6,
  /*LAT*/ PB7,
  /*CLK*/ PB8);

CircularBuffer buffer;

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

void printLine(uint8_t line, String message)
{
  // convert input, line, into x and y
  // line 1: x = 0, y = 0
  // line 2: x = 0, y = 8
  // line 3: x = 0, y = 16
  // line 4: x = 0, y = 24
  uint8_t linePixel = (line - 1) * CHAR_HEIGHT;
  for (uint8_t i = 0; i < message.length(); i++) {
      putch(i * CHAR_WIDTH, linePixel, message.charAt(i));
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

void initSpi()
{
  SPI.setModule(1);
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  SPI.beginSlave();
  spi_irq_enable(SPI.dev(), SPI_RXNE_INTERRUPT);
}

extern "C" {
  void __irq_spi1(void);
}

void __irq_spi1(void)
{
  uint16_t reg = spi_rx_reg(SPI.dev());
  spi_tx_reg(SPI.dev(), reg);

  uint8_t b = (uint8_t) (reg & 0xFF);
  buffer.put(b);
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
      noInterrupts();
      command_count--;
      interrupts();
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
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  initSpi();
  matrix.begin(displaybuf, WIDTH, HEIGHT);
  matrix.reverse();
  buffer.begin(bufferData, BUFF_LEN);
}

void loop()
{
  matrix.scan();

  if (command_count > 0) {
    uint8_t character;
    if (buffer.get(&character)) {
      process_character(character);
    }
  }
}

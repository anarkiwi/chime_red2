// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "CRLiquidCrystal.h"

CRLiquidCrystal::CRLiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
	uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3) {
  _rs_pin = rs;
  _rw_pin = rw;
  _enable_pin = enable;
  _data_pins[0] = d0;
  _data_pins[1] = d1;
  _data_pins[2] = d2;
  _data_pins[3] = d3;
  _busy_pin = _data_pins[3];

  pinMode(_rs_pin, OUTPUT);
  pinMode(_rw_pin, OUTPUT);
  pinMode(_enable_pin, OUTPUT);
  digitalWrite(_rs_pin, LOW);
  digitalWrite(_rw_pin, LOW);
  digitalWrite(_enable_pin, LOW);
  pinMode(_data_pins[0], OUTPUT);
  pinMode(_data_pins[1], OUTPUT);
  pinMode(_data_pins[2], OUTPUT);
  pinMode(_data_pins[3], OUTPUT);
}

void CRLiquidCrystal::begin() {
  writelow4(0x03);
  delay(50);

  for (uint8_t i = 0; i < 3; ++i) {
    writelow4(0x03);
    delay(5);
  }
  writelow4(0x02);
  delay(1);

  command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
  command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
  command(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
  clear();
}

void CRLiquidCrystal::clear() {
  command(LCD_CLEARDISPLAY);
}

void CRLiquidCrystal::setCursor(uint8_t col, uint8_t row) { // cppcheck-suppress unusedFunction
  byte offset = col;
  if (row) {
    offset |= 0x40;
  }
  command(LCD_SETDDRAMADDR | (byte) offset);
}

void CRLiquidCrystal::command(uint8_t value) {
  send(value, LOW);
}

void CRLiquidCrystal::write(uint8_t value) {
  send(value, HIGH);
}

void CRLiquidCrystal::enablePulse() {
  digitalWrite(_enable_pin, HIGH);
  digitalWrite(_enable_pin, LOW);
}

void CRLiquidCrystal::writehigh4(uint8_t value) {
  digitalWrite(_data_pins[0], value & 0x10);
  digitalWrite(_data_pins[1], value & 0x20);
  digitalWrite(_data_pins[2], value & 0x40);
  digitalWrite(_data_pins[3], value & 0x80);
  enablePulse();
}

void CRLiquidCrystal::writelow4(uint8_t value) {
  digitalWrite(_data_pins[0], value & 0x01);
  digitalWrite(_data_pins[1], value & 0x02);
  digitalWrite(_data_pins[2], value & 0x04);
  digitalWrite(_data_pins[3], value & 0x08);
  enablePulse();
}

void CRLiquidCrystal::waitFree() {
  pinMode(_busy_pin, INPUT);
  digitalWrite(_rw_pin, HIGH);
  digitalWrite(_rs_pin, LOW);
  bool busy = false;
  do {
    digitalWrite(_enable_pin, HIGH);
    busy = digitalRead(_busy_pin);
    digitalWrite(_enable_pin, LOW);
  } while (busy);
  pinMode(_busy_pin, OUTPUT);
  digitalWrite(_rw_pin, LOW);
}

void CRLiquidCrystal::send(uint8_t value, uint8_t mode) {
  waitFree();
  digitalWrite(_rs_pin, mode);
  writehigh4(value);
  writelow4(value);
}

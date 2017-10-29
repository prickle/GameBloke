#ifndef __PHYSICALDISPLAY_H
#define __PHYSICALDISPLAY_H

#include <string.h> // strncpy

#include "vmdisplay.h" // FIXME: for AiieRect

#define BLACK		0x0000 // 0 black
#define MAGENTA		0xC006 // 1 magenta
#define DARK_BLUE	0x0010 // 2 dark blue
#define PURPLE		0xA1B5 // 3 purple
#define DARK_GREEN	0x0480 // 4 dark green
#define DARK_GREY	0x6B4D // 5 dark grey
#define BLUE		0x1B9F // 6 med blue
#define LIGHT_BLUE	0x0DFD // 7 light blue
#define BROWN		0x92A5 // 8 brown
#define ORANGE		0xF8C5 // 9 orange
#define LIGHT_GREY	0x9555 // 10 light gray
#define PINK		0xFCF2 // 11 pink
#define GREEN		0x07E0 // 12 green
#define YELLOW		0xFFE0 // 13 yellow
#define AQUA		0x87F0 // 14 aqua
#define WHITE		0xFFFF  // 15 white

class PhysicalDisplay {
 public:
  PhysicalDisplay() { overlayMessage[0] = '\0'; }
  virtual ~PhysicalDisplay() {};

  virtual void redraw() = 0; // total redraw, assuming nothing
  virtual void blit(AiieRect r) = 0;   // redraw just the VM display area

  virtual void clrScr() = 0;
  virtual void clrScr(uint16_t color) = 0;
  virtual void clrScr(uint8_t r, uint8_t g, uint8_t b) = 0;

  virtual void drawDriveDoor(uint8_t which, bool isOpen) = 0;
  virtual void setDriveIndicator(uint8_t which, bool isRunning) = 0;
  virtual void drawBatteryStatus(uint8_t percent) = 0;

  virtual void setBackground(uint16_t color) = 0;
  virtual void drawCharacter(uint8_t mode, uint16_t x, uint8_t y, char c) = 0;
  virtual void drawString(uint8_t mode, uint16_t x, uint8_t y, const char *str) = 0;
  virtual void debugMsg(const char *msg) {   strncpy(overlayMessage, msg, sizeof(overlayMessage)); }
  
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) = 0;
  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) = 0;
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void fillRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;
  virtual void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) = 0;
  virtual void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;
  virtual void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color) = 0;
  virtual void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color) = 0;
  virtual void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color) = 0;
  virtual void drawThumb(int16_t x0, int16_t y0) = 0;   // draw the VM display area 1/2 size

  virtual void LED(uint8_t r, uint8_t g, uint8_t b) = 0;
 protected:
  char overlayMessage[40];
};

#endif

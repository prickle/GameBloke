#include <ctype.h> // isgraph
#include "teensy-display.h"

#include "bios-font.h"
#include "display-bg.h"

#define RS 16
#define WR 17
#define CS 18
#define RST 19

// Ports C&D of the Teensy connected to DB of the display
#define DB_0 15
#define DB_1 22
#define DB_2 23
#define DB_3 9
#define DB_4 10
#define DB_5 13
#define DB_6 11
#define DB_7 12
#define DB_8 2
#define DB_9 14
#define DB_10 7
#define DB_11 8
#define DB_12 6
#define DB_13 20
#define DB_14 21
#define DB_15 5

#define disp_x_size 239
#define disp_y_size 319

#define UPSIDEDOWN

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#endif

#define setPixel(color) { LCD_Write_DATA(((color)>>8),((color)&0xFF)); } // 565 RGB

#include "globals.h"
#include "applevm.h"

// RGB map of each of the lowres colors
const uint16_t loresPixelColors[16] = { 
					BLACK, // 0 black
					MAGENTA, // 1 magenta
					DARK_BLUE, // 2 dark blue
					PURPLE, // 3 purple
					DARK_GREEN, // 4 dark green
					DARK_GREY, // 5 dark grey
					BLUE, // 6 med blue
					LIGHT_BLUE, // 7 light blue
					BROWN, // 8 brown
					ORANGE, // 9 orange
					LIGHT_GREY, // 10 light gray
					PINK, // 11 pink
					GREEN, // 12 green
					YELLOW, // 13 yellow
					AQUA, // 14 aqua
					WHITE  // 15 white
};

uint16_t backgroundColor;

TeensyDisplay::TeensyDisplay()
{

  pinMode(DB_8, OUTPUT);
  pinMode(DB_9, OUTPUT);
  pinMode(DB_10, OUTPUT);
  pinMode(DB_11, OUTPUT);
  pinMode(DB_12, OUTPUT);
  pinMode(DB_13, OUTPUT);
  pinMode(DB_14, OUTPUT);
  pinMode(DB_15, OUTPUT);
  pinMode(DB_0, OUTPUT);
  pinMode(DB_1, OUTPUT);
  pinMode(DB_2, OUTPUT);
  pinMode(DB_3, OUTPUT);
  pinMode(DB_4, OUTPUT);
  pinMode(DB_5, OUTPUT);
  pinMode(DB_6, OUTPUT);
  pinMode(DB_7, OUTPUT);

  P_RS    = portOutputRegister(digitalPinToPort(RS));
  B_RS    = digitalPinToBitMask(RS);
  P_WR    = portOutputRegister(digitalPinToPort(WR));
  B_WR    = digitalPinToBitMask(WR);
  P_CS    = portOutputRegister(digitalPinToPort(CS));
  B_CS    = digitalPinToBitMask(CS);
  P_RST   = portOutputRegister(digitalPinToPort(RST));
  B_RST   = digitalPinToBitMask(RST);

  pinMode(RS,OUTPUT);
  pinMode(WR,OUTPUT);
  pinMode(CS,OUTPUT);
  pinMode(RST,OUTPUT);

  // begin initialization

  sbi(P_RST, B_RST);
  delay(5);
  cbi(P_RST, B_RST);
  delay(15);
  sbi(P_RST, B_RST);
  delay(15);

  cbi(P_CS, B_CS);

  LCD_Write_COM_DATA(0x00,0x0001); // oscillator start [enable]
  LCD_Write_COM_DATA(0x03,0xA8A4); // power control [%1010 1000 1010 1000] == DCT3, DCT1, BT2, DC3, DC1, AP2
  LCD_Write_COM_DATA(0x0C,0x0000); // power control2 [0]
  LCD_Write_COM_DATA(0x0D,0x080C); // power control3 [VRH3, VRH2, invalid bits]
  LCD_Write_COM_DATA(0x0E,0x2B00); // power control4 VCOMG, VDV3, VDV1, VDV0
  LCD_Write_COM_DATA(0x1E,0x00B7); // power control5 nOTP, VCM5, VCM4, VCM2, VCM1, VCM0
  //  LCD_Write_COM_DATA(0x01,0x2B3F); // driver control output REV, BGR, TB, MUX8, MUX5, MUX4, MUX3, MUX2, MUX1, MUX0
  // Change that: TB = 0 please
  LCD_Write_COM_DATA(0x01,0x293F); // driver control output REV, BGR, TB, MUX8, MUX5, MUX4, MUX3, MUX2, MUX1, MUX0
  //  LCD_Write_COM_DATA(0x01,0x693F); // driver control output RL, REV, BGR, TB, MUX8, MUX5, MUX4, MUX3, MUX2, MUX1, MUX0


  LCD_Write_COM_DATA(0x02,0x0600); // LCD drive AC control B/C, EOR                                                        
  LCD_Write_COM_DATA(0x10,0x0000); // sleep mode 0                                                                         
  // Change the (Y) order here to match above (TB=0)
  //LCD_Write_COM_DATA(0x11,0x6070); // Entry mode DFM1, DFM0, TY0, ID1, ID0
  //LCD_Write_COM_DATA(0x11,0x6050); // Entry mode DFM1, DFM0, TY0, ID0
  LCD_Write_COM_DATA(0x11,0x6078); // Entry mode DFM1, DFM0, TY0, ID1, ID0, AM

  LCD_Write_COM_DATA(0x05,0x0000); // compare reg1                                                                         
  LCD_Write_COM_DATA(0x06,0x0000); // compare reg2                                                                         
  LCD_Write_COM_DATA(0x16,0xEF1C); // horiz porch (default)                                                                
  LCD_Write_COM_DATA(0x17,0x0003); // vertical porch                                                                       
  LCD_Write_COM_DATA(0x07,0x0233); // display control VLE1, GON, DTE, D1, D0                                               
  LCD_Write_COM_DATA(0x0B,0x0000); // frame cycle control                                                                  
  LCD_Write_COM_DATA(0x0F,0x0000); // gate scan start posn                                                                 
  LCD_Write_COM_DATA(0x41,0x0000); // vertical scroll control1                                                             
  LCD_Write_COM_DATA(0x42,0x0000); // vertical scroll control2                                                             
  LCD_Write_COM_DATA(0x48,0x0000); // first window start                                                                   
  LCD_Write_COM_DATA(0x49,0x013F); // first window end (0x13f == 319)                                                      
  LCD_Write_COM_DATA(0x4A,0x0000); // second window start                                                                  
  LCD_Write_COM_DATA(0x4B,0x0000); // second window end                                                                    
  LCD_Write_COM_DATA(0x44,0xEF00); // horiz ram addr posn                                                                  
  LCD_Write_COM_DATA(0x45,0x0000); // vert ram start posn                                                                  
  LCD_Write_COM_DATA(0x46,0x013F); // vert ram end posn                                                                    
  LCD_Write_COM_DATA(0x30,0x0707); // γ control                                                                            
  LCD_Write_COM_DATA(0x31,0x0204);//                                                                                       
  LCD_Write_COM_DATA(0x32,0x0204);//                                                                                       
  LCD_Write_COM_DATA(0x33,0x0502);//                                                                                       
  LCD_Write_COM_DATA(0x34,0x0507);//                                                                                       
  LCD_Write_COM_DATA(0x35,0x0204);//                                                                                       
  LCD_Write_COM_DATA(0x36,0x0204);//                                                                                       
  LCD_Write_COM_DATA(0x37,0x0502);//                                                                                       
  LCD_Write_COM_DATA(0x3A,0x0302);//                                                                                       
  LCD_Write_COM_DATA(0x3B,0x0302);//                                                                                       
  LCD_Write_COM_DATA(0x23,0x0000);// RAM write data mask1                                                                  
  LCD_Write_COM_DATA(0x24,0x0000); // RAM write data mask2                                                                 
  LCD_Write_COM_DATA(0x25,0x8000); // frame frequency (OSC3)                                                               
  LCD_Write_COM_DATA(0x4f,0x0000); // Set GDDRAM Y address counter                                                         
  LCD_Write_COM_DATA(0x4e,0x0000); // Set GDDRAM X address counter                                                         
#if 0
  // Set data access speed optimization (?)
  LCD_Write_COM_DATA(0x28, 0x0006);
  LCD_Write_COM_DATA(0x2F, 0x12BE);
  LCD_Write_COM_DATA(0x12, 0x6CEB);
#endif

  LCD_Write_COM(0x22);   // RAM data write                                                                                 
  sbi(P_CS, B_CS);

  // LCD initialization complete

  setColor(255, 255, 255);

  clrScr();

  driveIndicator[0] = driveIndicator[1] = false;
  driveIndicatorDirty = true;
  
  backgroundColor = DARK_BLUE;
  LED(0, 0, 64);
}

TeensyDisplay::~TeensyDisplay()
{
}

void TeensyDisplay::LED(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(REDPIN, ledRed = r);
  analogWrite(GREENPIN, ledGreen = g);
  analogWrite(BLUEPIN, ledBlue = b);
}

void TeensyDisplay::LED() {
  analogWrite(REDPIN, ledRed);
  analogWrite(GREENPIN, ledGreen);
  analogWrite(BLUEPIN, ledBlue);
}

void TeensyDisplay::LEDflash() {
  analogWrite(REDPIN, 128);
  analogWrite(GREENPIN, 128);
  analogWrite(BLUEPIN, 128);
}

void TeensyDisplay::_fast_fill_16(int ch, int cl, long pix)
{
  *(volatile uint8_t *)(&GPIOD_PDOR) = ch;
  *(volatile uint8_t *)(&GPIOC_PDOR) = cl;
  uint16_t blocks = pix / 16;

  for (uint16_t i=0; i<blocks; i++) {
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
    pulse_low(P_WR, B_WR);
  }
  if ((pix % 16) != 0) {
    for (int i=0; i<(pix % 16); i++)
      {
	pulse_low(P_WR, B_WR);
      }
  }
}

void TeensyDisplay::redraw()
{
  cbi(P_CS, B_CS);
  clrXY();
  sbi(P_RS, B_RS);

  moveTo(0, 0);

#ifdef UPSIDEDOWN
  for (int y=TEENSY_DHEIGHT-1; y>=0; y--) {
    for (int x=TEENSY_DWIDTH-1; x>=0; x--) {
#else
  for (int y=0; y<TEENSY_DHEIGHT; y++) {
    for (int x=0; x<TEENSY_DWIDTH; x++) {
#endif	
   	  uint8_t r = pgm_read_byte(&displayBitmap[(y * DBITMAP_WIDTH + x)*3 + 0]);
      uint8_t g = pgm_read_byte(&displayBitmap[(y * DBITMAP_WIDTH + x)*3 + 1]);
      uint8_t b = pgm_read_byte(&displayBitmap[(y * DBITMAP_WIDTH + x)*3 + 2]);
      uint16_t color16 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
      setPixel(color16);
    }
  }

  if (g_vm) {
    drawDriveDoor(0, ((AppleVM *)g_vm)->DiskName(0)[0] == '\0');
    drawDriveDoor(1, ((AppleVM *)g_vm)->DiskName(1)[0] == '\0');
  }

  cbi(P_CS, B_CS);
  clrXY();
  sbi(P_RS, B_RS);
}

void TeensyDisplay::clrScr()
{
  cbi(P_CS, B_CS);
  clrXY();
  sbi(P_RS, B_RS);
  _fast_fill_16(0, 0, ((disp_x_size+1)*(disp_y_size+1)));
  sbi(P_CS, B_CS);
}

void TeensyDisplay::clrScr(uint16_t color)
{
  uint8_t ch = (uint8_t)(color >> 8);
  uint8_t cl = (uint8_t)(color & 0xFF);
  cbi(P_CS, B_CS);
  clrXY();
  sbi(P_RS, B_RS);
  _fast_fill_16(ch, cl, ((disp_x_size+1)*(disp_y_size+1)));
  sbi(P_CS, B_CS);
}

void TeensyDisplay::clrScr(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t ch=((r&248)|g>>5);
  uint8_t cl=((g&28)<<3|b>>3);
  cbi(P_CS, B_CS);
  clrXY();
  sbi(P_RS, B_RS);
  _fast_fill_16(ch, cl, ((disp_x_size+1)*(disp_y_size+1)));
  sbi(P_CS, B_CS);
}


// The display flips X and Y, so expect to see "x" as "vertical"
// and "y" as "horizontal" here...
void TeensyDisplay::setYX(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
 LCD_Write_COM_DATA(0x44, (y2<<8)+y1); // Horiz start addr, Horiz end addr
 LCD_Write_COM_DATA(0x45, x1); // vert start pos
 LCD_Write_COM_DATA(0x46, x2); // vert end pos
 LCD_Write_COM_DATA(0x4e,y1); // RAM address set (horiz) 
 LCD_Write_COM_DATA(0x4f,x1); // RAM address set (vert)
 LCD_Write_COM(0x22);
}

void TeensyDisplay::clrXY()
{
  setYX(0, 0, disp_y_size, disp_x_size);
}

void TeensyDisplay::setColor(byte r, byte g, byte b)
{
  fch=((r&248)|g>>5);
  fcl=((g&28)<<3|b>>3);
}

void TeensyDisplay::setColor(uint16_t color)
{
  fch = (uint8_t)(color >> 8);
  fcl = (uint8_t)(color & 0xFF);
}

void TeensyDisplay::fillRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
#ifdef UPSIDEDOWN
  x1 = disp_y_size - x1;
  x2 = disp_y_size - x2;
  y1 = disp_x_size - y1;
  y2 = disp_x_size - y2;
#endif

  if (x1>x2) {
    swap(uint16_t, x1, x2);
  }
  if (y1 > y2) {
    swap(uint16_t, y1, y2);
  }

  cbi(P_CS, B_CS);
  setYX(x1, y1, x2, y2);
  sbi(P_RS, B_RS);
  _fast_fill_16(fch,fcl,((long(x2-x1)+1)*(long(y2-y1)+1)));
  sbi(P_CS, B_CS);
}

void TeensyDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
 uint16_t color) {
  setColor(color);
  fillRect(x, y, x+w, y+h);
}

void TeensyDisplay::drawPixel(uint16_t x, uint16_t y)
{
//#ifdef UPSIDEDOWN
//  x = disp_y_size - x;
//  y = disp_x_size - y;
//#endif

  cbi(P_CS, B_CS);
  setYX(x, y, x, y);
  setPixel((fch<<8)|fcl);
  sbi(P_CS, B_CS);
  clrXY();
}

void TeensyDisplay::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
//#ifdef UPSIDEDOWN
//  x = disp_y_size - x;
//  y = disp_x_size - y;
//#endif

  cbi(P_CS, B_CS);
  setYX(x, y, x, y);
  setPixel(color);
  sbi(P_CS, B_CS);
  clrXY();
}

void TeensyDisplay::drawPixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t color16 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
//#ifdef UPSIDEDOWN
//  x = disp_y_size - x;
//  y = disp_x_size - y;
//#endif

  cbi(P_CS, B_CS);
  setYX(x, y, x, y);
  setPixel(color16);
  sbi(P_CS, B_CS);
  clrXY();
}


// Draw a circle outline
void TeensyDisplay::drawCircle(int16_t x0, int16_t y0, int16_t r,
 uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

#ifdef UPSIDEDOWN
  x0 = disp_y_size - x0;
  y0 = disp_x_size - y0;
#endif

  drawPixel(x0  , y0+r, color);
  drawPixel(x0  , y0-r, color);
  drawPixel(x0+r, y0  , color);
  drawPixel(x0-r, y0  , color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void TeensyDisplay::drawCircleHelper( int16_t x0, int16_t y0,
 int16_t r, uint8_t cornername, uint16_t color) {
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

#ifdef UPSIDEDOWN
  x0 = disp_y_size - x0;
  y0 = disp_x_size - y0;
#endif

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
#ifdef UPSIDEDOWN
    if (cornername & 0x1) {
#else
    if (cornername & 0x4) {
#endif
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    }
#ifdef UPSIDEDOWN
    if (cornername & 0x8) {
#else
    if (cornername & 0x2) {
#endif
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
#ifdef UPSIDEDOWN
    if (cornername & 0x2) {
#else
    if (cornername & 0x8) {
#endif
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
#ifdef UPSIDEDOWN
    if (cornername & 0x4) {
#else
    if (cornername & 0x1) {
#endif
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void TeensyDisplay::fillCircle(int16_t x0, int16_t y0, int16_t r,
 uint16_t color) {
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// Used to do circles and roundrects
void TeensyDisplay::fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
 uint8_t cornername, int16_t delta, uint16_t color) {

  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

// Bresenham's algorithm - thx wikpedia
void TeensyDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
 uint16_t color) {
#ifdef UPSIDEDOWN
  x0 = disp_y_size - x0;
  y0 = disp_x_size - y0;
  x1 = disp_y_size - x1;
  y1 = disp_x_size - y1;
#endif
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {
      drawPixel(y0, x0, color);
    } else {
      drawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

// Draw a rectangle
void TeensyDisplay::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
 uint16_t color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y+h-1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x+w-1, y, h, color);
}

void TeensyDisplay::drawFastVLine(int16_t x, int16_t y,
 int16_t h, uint16_t color) {
  // Update in subclasses if desired!
  setColor(color);
  fillRect(x, y, x, y+h-1);
}

void TeensyDisplay::drawFastHLine(int16_t x, int16_t y,
 int16_t w, uint16_t color) {
  // Update in subclasses if desired!
  setColor(color);
  fillRect(x, y, x+w-1, y);
}


// Draw a rounded rectangle
void TeensyDisplay::drawRoundRect(int16_t x, int16_t y, int16_t w,
 int16_t h, int16_t r, uint16_t color) {
  // smarter version
  drawFastHLine(x+r  , y    , w-2*r, color); // Top
  drawFastHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawFastVLine(x    , y+r  , h-2*r, color); // Left
  drawFastVLine(x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// Fill a rounded rectangle
void TeensyDisplay::fillRoundRect(int16_t x, int16_t y, int16_t w,
 int16_t h, int16_t r, uint16_t color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}


void TeensyDisplay::LCD_Writ_Bus(uint8_t ch, uint8_t cl)
{
  *(volatile uint8_t *)(&GPIOD_PDOR) = ch;
  *(volatile uint8_t *)(&GPIOC_PDOR) = cl;
  pulse_low(P_WR, B_WR);
}

void TeensyDisplay::LCD_Write_COM(uint8_t VL)
{
  cbi(P_RS, B_RS);
  LCD_Writ_Bus(0x00, VL);
}

void TeensyDisplay::LCD_Write_DATA(uint8_t VH, uint8_t VL)
{
  sbi(P_RS, B_RS);
  LCD_Writ_Bus(VH,VL);
}

void TeensyDisplay::LCD_Write_DATA(uint8_t VL)
{
  sbi(P_RS, B_RS);
  LCD_Writ_Bus(0x00, VL);
}

void TeensyDisplay::LCD_Write_COM_DATA(uint8_t com1, uint16_t dat1)
{
  LCD_Write_COM(com1);
  LCD_Write_DATA(dat1>>8, dat1);
}

void TeensyDisplay::moveTo(uint16_t col, uint16_t row)
{
  cbi(P_CS, B_CS);

  // FIXME: eventually set drawing to the whole screen and leave it that way

  // set drawing to the whole screen
  setYX(0, 0, disp_y_size, disp_x_size);
  LCD_Write_COM_DATA(0x4e,row); // RAM address set (horiz) 
  LCD_Write_COM_DATA(0x4f,col); // RAM address set (vert)

  LCD_Write_COM(0x22);
}

void TeensyDisplay::drawNextPixel(uint16_t color)
{
  // Anything inside this object should call setPixel directly. This
  // is primarily for the BIOS.
  setPixel(color);
}

void TeensyDisplay::blit(AiieRect r)
{
  uint8_t *videoBuffer = g_vm->videoBuffer; // FIXME: poking deep
  // remember these are "starts at pixel number" values, where 0 is the first.
  #define HOFFSET 18
  #define VOFFSET 13

  // Define the horizontal area that we're going to draw in
#ifdef UPSIDEDOWN
  LCD_Write_COM_DATA(0x44, ((disp_x_size-VOFFSET-r.top)<<8)+disp_x_size-VOFFSET-r.bottom); // Horiz start addr, Horiz end addr
  LCD_Write_COM_DATA(0x45, disp_y_size-HOFFSET-r.right); // offset by 20 to center it...
  LCD_Write_COM_DATA(0x46, disp_y_size-HOFFSET-r.left);
#else
  LCD_Write_COM_DATA(0x44, ((VOFFSET+r.top)<<8)+VOFFSET+r.bottom);
  LCD_Write_COM_DATA(0x45, HOFFSET+r.left); // offset by 20 to center it...
  LCD_Write_COM_DATA(0x46, HOFFSET+r.right);
#endif

  // position the "write" address
#ifdef UPSIDEDOWN
  LCD_Write_COM_DATA(0x4e,disp_x_size-VOFFSET-r.bottom); // row
  LCD_Write_COM_DATA(0x4f,disp_y_size-HOFFSET-r.right); // col
#else
  LCD_Write_COM_DATA(0x4e,VOFFSET+r.top); // row
  LCD_Write_COM_DATA(0x4f,HOFFSET+r.left); // col
#endif

  // prepare the LCD to receive data bytes for its RAM
  LCD_Write_COM(0x22);

  // send the pixel data
  sbi(P_RS, B_RS);
  uint16_t pixel;
#ifdef UPSIDEDOWN
  for (int16_t y=r.bottom; y>=r.top; y--) {
    for (int16_t x=r.right; x>=r.left; x--) {
#else
  for (uint8_t y=r.top; y<=r.bottom; y++) {
    for (uint16_t x=r.left; x<=r.right; x++) {
#endif
      pixel = y * (DISPLAYRUN >> 1) + (x >> 1);
      uint8_t colorIdx;
      if (!(x & 0x01)) {
	colorIdx = videoBuffer[pixel] >> 4;
      } else {
	colorIdx = videoBuffer[pixel] & 0x0F;
      }
      LCD_Writ_Bus(loresPixelColors[colorIdx]>>8,loresPixelColors[colorIdx]);
    }
  }
  cbi(P_CS, B_CS);

  // draw overlay, if any
  if (overlayMessage[0]) {
    // reset the viewport in order to draw the overlay...
    LCD_Write_COM_DATA(0x45, 0);
    LCD_Write_COM_DATA(0x46, 319);
  
    drawString(M_SELECTDISABLED, 1, 240 - 16 - 12, overlayMessage);
  }

  redrawDriveIndicators();
}


void TeensyDisplay::drawThumb(int16_t x0, int16_t y0) {   // draw the VM display area 1/2 size
  uint8_t *videoBuffer = g_vm->videoBuffer; // FIXME: poking deep
  #define HSIZE 139
  #define VSIZE 95
  
  int16_t x1 = x0 + HSIZE; 
  int16_t y1 = y0 + VSIZE; 
  cbi(P_CS, B_CS);
  clrXY();
  sbi(P_RS, B_RS);

  // Define the horizontal area that we're going to draw in
#ifdef UPSIDEDOWN
  LCD_Write_COM_DATA(0x44, ((disp_x_size-y0)<<8)+disp_x_size-y1); // Horiz start addr, Horiz end addr
  LCD_Write_COM_DATA(0x45, disp_y_size-x1); // offset by 20 to center it...
  LCD_Write_COM_DATA(0x46, disp_y_size-x0);
#else
  LCD_Write_COM_DATA(0x44, ((y0)<<8)+y1);
  LCD_Write_COM_DATA(0x45, x0); // offset by 20 to center it...
  LCD_Write_COM_DATA(0x46, x1);
#endif

  // position the "write" address
#ifdef UPSIDEDOWN
  LCD_Write_COM_DATA(0x4e,disp_x_size-y1); // row
  LCD_Write_COM_DATA(0x4f,disp_y_size-x1); // col
#else
  LCD_Write_COM_DATA(0x4e,y0); // row
  LCD_Write_COM_DATA(0x4f,x0); // col
#endif

  // prepare the LCD to receive data bytes for its RAM
  LCD_Write_COM(0x22);

  // send the pixel data
  sbi(P_RS, B_RS);
  uint16_t pixel1, pixel2;
#ifdef UPSIDEDOWN
  for (int16_t y=VSIZE; y>=0; y--) {
    for (int16_t x=HSIZE; x>=0; x--) {
#else
  for (uint8_t y=0; y<=VSIZE; y++) {
    for (uint16_t x=0; x<=HSIZE; x++) {
#endif
      pixel1 = y * DISPLAYRUN + x;
      pixel2 = (y * DISPLAYRUN) + (DISPLAYRUN >> 1) + x;
      uint8_t colorIdx = (videoBuffer[pixel1] >> 4) | (videoBuffer[pixel1] & 0x0F);
	  colorIdx |= (videoBuffer[pixel2] >> 4) | (videoBuffer[pixel2 ] & 0x0F);
      LCD_Writ_Bus(loresPixelColors[colorIdx]>>8,loresPixelColors[colorIdx]);
    }
  }
  cbi(P_CS, B_CS);
}




void TeensyDisplay::setBackground(uint16_t color) {
	backgroundColor = color;
}

void TeensyDisplay::drawCharacter(uint8_t mode, uint16_t x, uint8_t y, char c)
{
  int8_t xsize = 8,
    ysize = 0x0C,
    offset = 0x20;
  uint16_t temp;

  c -= offset;// font starts with a space

  uint16_t offPixel, onPixel;
  switch (mode) {
  case M_NORMAL:
    onPixel = WHITE;
    offPixel = backgroundColor;
    break;
  case M_SELECTED:
    onPixel = BLACK;
    offPixel = WHITE;
    break;
  case M_DISABLED:
  default:
    onPixel = DARK_GREY;
    offPixel = backgroundColor;
    break;
  case M_SELECTDISABLED:
    onPixel = BLACK;
    offPixel = DARK_GREY;
    break;
  case M_HIGHLIGHT:
    onPixel = GREEN;
    offPixel = backgroundColor;
    break;
  case M_SELECTHIGHLIGHT:
    onPixel = BLACK;
    offPixel = YELLOW;
    break;
  }

#ifdef UPSIDEDOWN
  temp=(c*ysize+ysize);
  for (int8_t y_off = ysize; y_off >= 0; y_off--) {
    moveTo(disp_y_size-(x+xsize), disp_x_size-(y + y_off));
    uint8_t ch = pgm_read_byte(&BiosFont[temp]);
    for (int8_t x_off = xsize; x_off >= 0; x_off--) {
      if (ch & (1 << (7-x_off))) {
	setPixel(onPixel);
      } else {
	setPixel(offPixel);
      }
    }
    temp--;
  }
#else
  temp=(c*ysize);
  for (int8_t y_off = 0; y_off <= ysize; y_off++) {
    moveTo(x, y + y_off);
    uint8_t ch = pgm_read_byte(&BiosFont[temp]);
    for (int8_t x_off = 0; x_off <= xsize; x_off++) {
      if (ch & (1 << (7-x_off))) {
	setPixel(onPixel);
      } else {
	setPixel(offPixel);
      }
    }
    temp++;
  }
#endif
}

void TeensyDisplay::drawString(uint8_t mode, uint16_t x, uint8_t y, const char *str)
{
  int8_t xsize = 8; // width of a char in this font

  for (uint8_t i=0; i<strlen(str); i++) {
    drawCharacter(mode, x, y, str[i]);
    x += xsize; // fixme: any inter-char spacing?
  }
}

void TeensyDisplay::drawDriveDoor(uint8_t which, bool isOpen)
{
  // location of drive door for left drive

  uint16_t xoff = 55;
  uint16_t yoff = 216;

  // location for right drive

  if (which == 1) {
    xoff += 134;
  }

  for (int y=0; y<20; y++) {
    for (int x=0; x<43; x++) {
      uint8_t r, g, b;
      if (isOpen) {
	r = pgm_read_byte(&driveLatchOpen[(y * 43 + x)*3 + 0]);
	g = pgm_read_byte(&driveLatchOpen[(y * 43 + x)*3 + 1]);
	b = pgm_read_byte(&driveLatchOpen[(y * 43 + x)*3 + 2]);
      } else {
	r = pgm_read_byte(&driveLatch[(y * 43 + x)*3 + 0]);
	g = pgm_read_byte(&driveLatch[(y * 43 + x)*3 + 1]);
	b = pgm_read_byte(&driveLatch[(y * 43 + x)*3 + 2]);
      }
#ifdef UPSIDEDOWN
  	  drawPixel(disp_y_size - (x + xoff), disp_x_size - (y + yoff), r, g, b);
#else
      drawPixel(x+xoff, y+yoff, r, g, b);
#endif
    }
  }
}

void TeensyDisplay::setDriveIndicator(uint8_t which, bool isRunning)
{
  driveIndicator[which] = isRunning;
  driveIndicatorDirty = true;
  
  if (driveIndicator[0] || driveIndicator[1]) LEDflash();
  else LED();
}

void TeensyDisplay::redrawDriveIndicators()
{
  if (driveIndicatorDirty) {
    // location of status indicator for left drive                                
    uint16_t xoff = 125;
    uint16_t yoff = 213;
    
    for (int which = 0; which <= 1; which++,xoff += 135) {
      
      for (int y=0; y<2; y++) {
	for (int x=0; x<6; x++) {
#ifdef UPSIDEDOWN
  	  drawPixel(disp_y_size - (x + xoff), disp_x_size - (y + yoff), driveIndicator[which] ? 0xF800 : 0x8AA9);
#else
	  drawPixel(x + xoff, y + yoff, driveIndicator[which] ? 0xF800 : 0x8AA9);
#endif
	}
      }
    }
    driveIndicatorDirty = false;
  }
}

void TeensyDisplay::drawBatteryStatus(uint8_t percent)
{
  uint16_t xoff = 300;
  uint16_t yoff = 222;

  // the area around the apple is 12 wide
  // it's exactly 11 high
  // the color is 210/202/159

  float watermark = ((float)percent / 100.0) * 11;

  for (int y=0; y<11; y++) {
    uint8_t bgr = 210;
    uint8_t bgg = 202;
    uint8_t bgb = 159;
    if (11-y > watermark) {
      // black...
      bgr = bgg = bgb = 0;
    }

    for (int x=0; x<11; x++) {
      //uint8_t *p = &appleBitmap[(y * 10 + (x-1))*4];
      // It's RGBA; blend w/ background color

      uint8_t r,g,b;
      float alpha = (float)pgm_read_byte(&appleBitmap[(y * 10 + (x-1))*4 + 3]) / 255.0;

      r = (float)pgm_read_byte(&appleBitmap[(y * 10 + (x-1))*4 + 0]) 
	* alpha + (bgr * (1.0 - alpha));
      g = (float)pgm_read_byte(&appleBitmap[(y * 10 + (x-1))*4 + 1]) 
	* alpha + (bgr * (1.0 - alpha));
      b = (float)pgm_read_byte(&appleBitmap[(y * 10 + (x-1))*4 + 2]) 
	* alpha + (bgr * (1.0 - alpha));


#ifdef UPSIDEDOWN
  	  drawPixel(disp_y_size - (x + xoff), disp_x_size - (y + yoff), r, g, b);
#else
      drawPixel(x+xoff, y+yoff, r, g, b);
#endif
    }
  }
}


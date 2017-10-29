#include "bios.h"

#include "applevm.h"
#include "physicalkeyboard.h"
#include "teensy-usb-keyboard.h"
#include "cpu.h"
#include "teensy-filemanager.h"
#include "teensy-display.h"
#include "widgets.h"

#include "globals.h"

enum {
  ACT_EXIT = 0,
  ACT_RESET = 1,
  ACT_REBOOT = 2,
  ACT_DISK1 = 3,
  ACT_DISK2 = 4,
  ACT_VOLUME = 5,
  ACT_SAVESTATE = 6,
  ACT_RESTORESTATE = 7,
  ACT_MONITOR = 8,
  ACT_DISPLAYTYPE = 9,
  ACT_SYNC = 10,
  ACT_DEBUG = 11,
  ACT_JOYMODE = 12,
  ACT_JOYTRIM = 13,
  ACT_KEYPRESETS = 14,

  NUM_ACTIONS = 15
};

static const char *titles[NUM_ACTIONS] = { "Resume",
				    "Reset",
				    "Cold Reboot",
				    "%s Disk 1",
				    "%s Disk 2",
				    "Volume",
				    "Save State",
				    "Restore State",
				    "Drop to Monitor",
				    "Display: %s",
				    "Sync: %s",
				    "Debug: %s",
				    "Stick: %s",
				    "Stick %s",
				    "Keyboard Preset"
};

static const char* KeyNames[141] = 
{
   "\x95\x96","\x81\x41","\x81\x42","\x81\x43","\x81\x44","\x81\x45","\x81\x46","\x81G",
   "\x8B","\x80","\x8E","\x8D","\x81L","\x8F","\x81N","\x81O",
   "\x81P","\x81Q","\x81R","\x81S","\x81T","\x8C","\x81V","\x81W",
   "\x81X","\x81Y","\x81Z","\x7F","28","29","30","31",						//32
   "\x91\x92","!","\"","#","$","%","&","\'","(",")","*","+",",","-",".","/",	//48
   "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?","@",		//66
   "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q",		//83
   "R","S","T","U","V","W","X","Y","Z","[","\\","]","^","_","'","a","b",	//100
   "c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s",		//117
   "t","u","v","w","x","y","z","{","|","}","~","\x7F","?",						//128
   "\x81","L\x82\x63","R\x82\x83","\x93\x94","\x85","\x86","\x90","\x87","\x88","\x89","\x8A"
};

// FIXME: abstract the pin # rather than repeating it here
#define RESETPIN 39
#define BATTERYPIN A15
#define CHARGEPIN A16

extern int16_t g_volume; // FIXME: external global. icky.
extern int8_t debugMode; // and another. :/
int8_t joyMode;
Widgets widgets;

// FIXME: and these need abstracting out of the main .ino !
enum {
  D_NONE        = 0,
  D_SHOWFPS     = 1,
  D_SHOWMEMFREE = 2,
  D_SHOWPADDLES = 3,
  D_SHOWPC      = 4,
  D_SHOWCYCLES  = 5,
  D_SHOWBATTERY = 6,
  D_SHOWTIME    = 7
};

const char *staticPathConcat(const char *rootPath, const char *filePath)
{
  static char buf[MAXPATH];
  strncpy(buf, rootPath, sizeof(buf)-1);
  strncat(buf, filePath, sizeof(buf)-strlen(buf)-1);

  return buf;
}


BIOS::BIOS()
{
  strcpy(rootPath, "/");

  selectedFile = -1;
  for (int8_t i=0; i<BIOS_MAXFILES; i++) {
    // Put end terminators in place; strncpy won't copy over them
     fileDirectory[i][BIOS_MAXPATH] = '\0';
  }
  inMainMenu = false;
  dirPage = 0;
  joyMode = JOY_MODE_ANA_ABS;
}

BIOS::~BIOS()
{
}

#define BATT_CHG	0
#define BATT_FULL	1
#define BATT_OK		2
#define BATT_LOW	3
#define BATT_OUT	4


void updateLED() {
  static uint8_t batState = BATT_CHG;
  static uint8_t state;
  if (g_charge > 15) state = BATT_CHG; //charging
  else {
    if (g_battery > 3400) state = BATT_OK; //Good
    else if (g_battery > 3200) state = BATT_LOW; //Low
    else if (g_battery > 1000) state = BATT_OUT; //Expired
    else state = BATT_FULL; //Fully charged (off)
  }
  if (state != batState) {
    if (state == BATT_CHG) g_display->LED(0, 0, 64);
    else if (state == BATT_OK) g_display->LED(0, 32, 0); //Good
    else if (state == BATT_LOW) g_display->LED(32, 32, 0); //Low
    else if (state == BATT_OUT) g_display->LED(32, 0, 0); //Expired
    else g_display->LED(0, 32, 0); //Fully charged (off)
	batState = state;
  }
}

bool BIOS::updateDiagnostics() {
  static unsigned long nextBattCheck = 0;
  static float battery = 0;
  static float charge = 0;
  if (millis() >= nextBattCheck) {
    // FIXME: what about rollover?
    nextBattCheck = millis() + 500; //30 * 1000; // check every 30 seconds

    //Lightly filtered
    battery = analogRead(BATTERYPIN) / 2 + battery / 2;
    charge = analogRead(CHARGEPIN) / 2 + charge / 2 ;
    //vccLevel = 1195 * 4096 /analogRead(39);
    //analogReference(INTERNAL);
    //cpuTemp = 25.0 + 0.17083 * (2454.19 - analogRead(38));
    //analogReference(DEFAULT);

    g_battery = battery * 1.685f; //mV
    g_charge = charge * 0.206f; //mA
	updateLED();
	return true;
  }
  return false;
}

void resetJoyMode() {
  if (joyMode == JOY_MODE_JOYPORT1 || joyMode == JOY_MODE_JOYPORT2) {
    joyMode = JOY_MODE_ANA_ABS;
    g_keyboard->setJoymode(joyMode);
  }
}

void BIOS::drawBiosScreen(int8_t prevAction) {
  g_display->clrScr(210,202,159); //(0x0010);
  g_display->fillRect(0, 0, 319, 15, DARK_BLUE);
  g_display->drawLine(0,16, 320, 16, WHITE);
  g_display->drawString(M_NORMAL, 2, 2, "BIOS Configuration");
  g_display->fillRoundRect(4, 24, 132, 213, 6, DARK_BLUE);
  g_display->drawRoundRect(4, 24, 132, 213, 6, WHITE);
  drawKeyboard();
  drawThumb();
  drawInfo();
  DrawMainMenu(prevAction);	
}

int8_t savedSelection = ACT_EXIT;
bool BIOS::runUntilDone()
{
  int8_t prevAction = savedSelection;
  int8_t key = 0;
  bool settingsDidChange = 0;
  drawBiosScreen(prevAction);
  while (1) {
	inMainMenu = true;
	key = GetAction(prevAction);
	inMainMenu = false;
    switch (prevAction) {
    case ACT_EXIT:
      goto done;
    case ACT_REBOOT:
	  resetJoyMode();
      ColdReboot();
      goto done;
    case ACT_RESET:
	  resetJoyMode();
      WarmReset();
      goto done;
    case ACT_MONITOR:
      ((AppleVM *)g_vm)->Monitor();
      goto done;
    case ACT_DISPLAYTYPE:
	  if (key == LARR) {  
        g_displayType--;
        if (g_displayType < 0) g_displayType = 3;
	  } else {
        g_displayType++;
	    g_displayType %= 4; // FIXME: abstract max #
      }
	  ((AppleDisplay*)g_display)->displayTypeChanged();
      break;
    case ACT_DEBUG:
	  if (key == LARR) {  
        debugMode--;
        if (debugMode < 0) debugMode = 7;
	  } else {
        debugMode++;
        debugMode %= 8; // FIXME: abstract max #
      }
	  break;
    case ACT_DISK1:
      if (((AppleVM *)g_vm)->DiskName(0)[0] != '\0') {
	    ((AppleVM *)g_vm)->ejectDisk(0, false);
		drawInfo();
      } else {
	    if (SelectDiskImage()) {
	      ((AppleVM *)g_vm)->insertDisk(0, staticPathConcat(rootPath, fileDirectory[selectedFile]), false);
	      goto done;
	    }
		drawBiosScreen(prevAction);
      }
      break;
    case ACT_DISK2:
        if (((AppleVM *)g_vm)->DiskName(1)[0] != '\0') {
	      ((AppleVM *)g_vm)->ejectDisk(1, false);
        } else {
	      if (SelectDiskImage()) {
	        ((AppleVM *)g_vm)->insertDisk(1, staticPathConcat(rootPath, fileDirectory[selectedFile]), false);
	        goto done;
	      }
		drawBiosScreen(prevAction);
        }
        break;
    case ACT_VOLUME:
	  if (key == RARR) {  
        g_volume ++;
        if (g_volume > 15) g_volume = 15;
        settingsDidChange = true;
	  }
	  if (key == LARR) {  
        g_volume--;
        if (g_volume < 0) g_volume = 0;
        settingsDidChange = true;
	  }
      break;
    case ACT_SYNC:
	  g_screenSync = !g_screenSync;
	  settingsDidChange = true;
	  break;
    case ACT_JOYMODE:
	  if (key == LARR) {  
        joyMode--;
        if (joyMode < 0) joyMode = 3;
		g_keyboard->setJoymode(joyMode);
		ClearMenuItem((int)ACT_JOYTRIM);
		DrawMenuItem((int)ACT_JOYTRIM, prevAction);
	  } else {
        joyMode++;
	    if (joyMode > 3) joyMode = 0; // FIXME: abstract max #
		g_keyboard->setJoymode(joyMode);
		ClearMenuItem((int)ACT_JOYTRIM);
		DrawMenuItem((int)ACT_JOYTRIM, prevAction);
      }
      break;
    case ACT_JOYTRIM:
      if (joyMode == JOY_MODE_ANA_ABS) {
	    if (stickTrim()) settingsDidChange = true;
	  } else {
	    if (stickSpeed()) settingsDidChange = true;
	  }
      break;
    case ACT_KEYPRESETS:
      break;
    case ACT_SAVESTATE:
	  saveState();
      break;
    case ACT_RESTORESTATE:
	  restoreState();
	  goto done;
    }
  }

 done:
  // Undo whatever damage we've done to the screen
  g_display->redraw();
  g_display->blit({0, 0, 191, 279});

  // return true if any persistent setting changed that we want to store in eeprom
  return settingsDidChange;
}

void BIOS::WarmReset()
{
  g_cpu->Reset();
}

void BIOS::ColdReboot()
{
  g_vm->Reset();
  g_cpu->Reset();
}

const char KeyMappings[5][15] = 
{
	{ LJOY, RJOY, UJOY, DJOY, LA, RA, 'j', 'k', '1', UARR, '2', RET, LARR, DARR, RARR },
	{ LJOY, RJOY, UJOY, DJOY, LA, RA, 'a', 'b', 'c', 'd', 'e', RET, 'f', 'g', 'h' },
	{ LJOY, RJOY, UJOY, DJOY, LA, RA, '1', '2', 'u', 'i', 'o', RET, 'j', 'k', 'l' },
	{ LARR, RARR, UARR, DARR, ' ', RET, 'p', 'k', '1', UARR, '2', ' ', LARR, DARR, RARR },
	{ LJOY, RJOY, UJOY, DJOY, LA, RA, 'y', 'n', '1', '2', '3', RET, '4', '5', '6' },	
};

void loadMap(uint8_t index) {
	for (int count = 0; count < 15; count++) {
		g_keyboard->setMapping(count, KeyMappings[index][count]);
	}
}


void BIOS::biosIdle(){
  static uint8_t batState = BATT_FULL;
  if(updateDiagnostics()) {
	if (inMainMenu) {
      uint8_t state;
      if (g_charge > 15) state = BATT_CHG; //charging
      else if (g_battery > 1000) state = BATT_OK; //Discharging
      else state = BATT_FULL; //Fully charged (off)
      if (state != batState || state == BATT_OK) {
	    widgets.drawBatteryText();
		batState = state;
      }
	}
  }
  yield();
}

void BIOS::drawKeyboard() {
  //g_display->drawRoundRect(4, 180, 312, 58, 6, WHITE);
  widgets.drawBattery(258, 140, WHITE);
  g_display->setBackground(BLACK);
  widgets.drawStick(140, 140, 48, 44, WHITE, KeyNames[g_keyboard->getMapping(0)], KeyNames[g_keyboard->getMapping(1)], KeyNames[g_keyboard->getMapping(2)], KeyNames[g_keyboard->getMapping(3)]);
  widgets.drawButton(192, 155, WHITE, KeyNames[g_keyboard->getMapping(4)]);	
  widgets.drawButton(222, 141, WHITE, KeyNames[g_keyboard->getMapping(5)]);	
  widgets.drawKey(140, 188, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(6)]);
  widgets.drawKey(176, 188, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(7)]);
  widgets.drawKey(212, 188, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(8)]);
  widgets.drawKey(248, 188, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(9)]);
  widgets.drawKey(284, 188, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(10)]);
	
  widgets.drawKey(140, 214, 68, 22, WHITE, KeyNames[g_keyboard->getMapping(11)]);
  widgets.drawKey(212, 214, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(12)]);
  widgets.drawKey(248, 214, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(13)]);
  widgets.drawKey(284, 214, 32, 22, WHITE, KeyNames[g_keyboard->getMapping(14)]);
  g_display->setBackground(DARK_BLUE);
}

#define INFO_SIZE 22
void BIOS::drawInfo(){
  char infoBuf[INFO_SIZE];  
  g_display->fillRect(141, 124, 8*INFO_SIZE, 13, DARK_BLUE);
  g_display->drawRect(140, 123, 8*INFO_SIZE+2, 15, WHITE);
  if (((AppleVM *)g_vm)->DiskName(0)[0] != '\0') {
	  char* start = strrchr(((AppleVM *)g_vm)->DiskName(0), '/') + 1;
	  int8_t length = strrchr(((AppleVM *)g_vm)->DiskName(0), '.') - start;
	  if (length > INFO_SIZE - 1) length = INFO_SIZE - 1;
	  if(start && length > 0) {
	    strncpy(infoBuf, start, length);
		infoBuf[length] = '\0';
        g_display->drawString(M_NORMAL, 141, 124, "\x97");
        g_display->drawString(M_NORMAL, 151, 124, infoBuf);
	  }
  } 
/*  if (((AppleVM *)g_vm)->DiskName(1) != '\0') { 
	  strncpy(infoBuf, ((AppleVM *)g_vm)->DiskName(1), INFO_SIZE-1);
      g_display->drawString(M_NORMAL, 150, 48, infoBuf);
  }*/
}

void BIOS::drawThumb(){
  g_display->drawRect(157, 24, 142, 98, WHITE);
  g_display->drawThumb(158, 25);
}

uint8_t BIOS::GetAction(int8_t& selection)
{
  uint8_t key;
  while (1) {
    UpdateMainMenu(selection);
    while (!g_keyboard->kbhit() && digitalRead(RESETPIN) == HIGH) {
      biosIdle();
      // Wait for either a keypress or the reset button to be pressed
    }

    if (digitalRead(RESETPIN) == LOW) {
      // wait until it's no longer pressed
      while (digitalRead(RESETPIN) == HIGH)
	;
      delay(100); // wait long enough for it to debounce
      // then return an exit code
	  selection = ACT_EXIT;
	  return SYSRQ;
    }
    key = g_keyboard->read();
    switch (key) {
    case SYSRQ:
	  selection = ACT_EXIT;
	  return key;
    case DARR:
      selection++;
      selection %= NUM_ACTIONS;
      break;
    case UARR:
      selection--;
      if (selection < 0) selection = NUM_ACTIONS-1;
      break;
    case RET:
    case LARR:
    case RARR:
      if (isActionActive(selection)) return key;
      break;
	case '1':
	  loadMap(0);
	  drawKeyboard();
      break;
    case '2':
	  loadMap(1);
	  drawKeyboard();
      break;
    case '3':
	  loadMap(2);
	  drawKeyboard();
      break;
    case '4':
	  loadMap(3);
	  drawKeyboard();
      break;
    case '5':
	  loadMap(4);
	  drawKeyboard();
      break;
    }
	savedSelection = selection;
  }
}

bool BIOS::isActionActive(int8_t action)
{
  // don't return true for disk events that aren't valid
  switch (action) {
  case ACT_EXIT:
  case ACT_RESET:
  case ACT_REBOOT:
  case ACT_MONITOR:
  case ACT_DISPLAYTYPE:
  case ACT_SYNC:
  case ACT_DEBUG:
  case ACT_DISK1:
  case ACT_DISK2:
  case ACT_VOLUME:
  case ACT_JOYMODE:
  case ACT_KEYPRESETS:
  case ACT_SAVESTATE:
  case ACT_RESTORESTATE:
    return true;
  case ACT_JOYTRIM:
    return (joyMode == JOY_MODE_ANA_ABS || joyMode == JOY_MODE_ANA_REL);
  }

  /* NOTREACHED */
  return false;
}

void BIOS::DrawMainMenu(int8_t selection)
{
  g_display->fillRect(7, 27, 126, 207, DARK_BLUE);
  for (int i=0; i<NUM_ACTIONS; i++) {
    DrawMenuItem(i, selection);
  }
}

void BIOS::UpdateMainMenu(int8_t selection)
{
  static int8_t oldSelection = -1;
  if (selection != oldSelection) {
    if (oldSelection >= 0) {
	  ClearMenuItem(oldSelection);
	  DrawMenuItem(oldSelection, selection);
	}
	oldSelection = selection;	
  }
  ClearMenuItem(selection);
  DrawMenuItem(selection, selection);
}

void BIOS::DrawMenuItem(int8_t item, int8_t selection)
{
  char buf[25];
  if (item == ACT_DISK1 || item == ACT_DISK2) {
    sprintf(buf, titles[item], ((AppleVM *)g_vm)->DiskName(item - ACT_DISK1)[0] ? "Eject" : "Insert");
  } else if (item == ACT_DISPLAYTYPE) {
    switch (g_displayType) {
      case m_blackAndWhite:
	    sprintf(buf, titles[item], "B&W");
	    break;
      case m_monochrome:
	    sprintf(buf, titles[item], "Mono");
	    break;
      case m_ntsclike:
	    sprintf(buf, titles[item], "NTSC");
	    break;
      case m_perfectcolor:
	    sprintf(buf, titles[item], "RGB");
	    break;
    }
  } else if (item == ACT_SYNC) {
    if (g_screenSync) {
 	  sprintf(buf, titles[item], "Screen");
	} else {
	  sprintf(buf, titles[item], "Sound");
    }
  } else if (item == ACT_DEBUG) {
    switch (debugMode) {
      case D_NONE:
		sprintf(buf, titles[item], "off");
		break;
      case D_SHOWFPS:
		sprintf(buf, titles[item], "FPS");
		break;
      case D_SHOWMEMFREE:
		sprintf(buf, titles[item], "FreeMem");
		break;
      case D_SHOWPADDLES:
		sprintf(buf, titles[item], "Paddles");
		break;
      case D_SHOWPC:
		sprintf(buf, titles[item], "PC");
		break;
      case D_SHOWCYCLES:
		sprintf(buf, titles[item], "Cycles");
		break;
      case D_SHOWBATTERY:
		sprintf(buf, titles[item], "Battery");
		break;
      case D_SHOWTIME:
		sprintf(buf, titles[item], "Time");
		break;
      }
  } else if (item == ACT_JOYMODE) {
    switch (joyMode) {
      case JOY_MODE_ANA_ABS:
		sprintf(buf, titles[item], "ana ABS");
		break;
      case JOY_MODE_ANA_REL:
		sprintf(buf, titles[item], "ana REL");
		break;
      case JOY_MODE_JOYPORT1:
		sprintf(buf, titles[item], "joyport1");
		break;
      case JOY_MODE_JOYPORT2:
		sprintf(buf, titles[item], "joyport2");
		break;
      }
  } else if (item == ACT_JOYTRIM) {
    if (joyMode == JOY_MODE_ANA_REL) {
	  sprintf(buf, titles[item], "Speed");		
	} else {
	  sprintf(buf, titles[item], "Trim");
	}	
  } else {
    strcpy(buf, titles[item]);
  }

  if (isActionActive(item)) {
    g_display->drawString(selection == item ? M_SELECTHIGHLIGHT : M_NORMAL, 10, 30 + 13 * item, buf);
  } else {
    g_display->drawString(selection == item ? M_SELECTDISABLED : M_DISABLED, 10, 30 + 13 * item, buf);
  }

  // draw the volume bar
  if (item == ACT_VOLUME) {
    uint16_t volCutoff = 59.0 * (float)((float) g_volume / 15.0);
    g_display->drawRoundRect(66,32 + 13 * ACT_VOLUME,64,10,3,selection == item ? YELLOW : WHITE);
    g_display->fillRect(68,34 + 13 * ACT_VOLUME,volCutoff,5,selection == item ? YELLOW : WHITE);
  }
}

void BIOS::ClearMenuItem(int8_t item)
{
  g_display->fillRect(10, 30 + 13 * item, 120, 12, DARK_BLUE);
}

void drawCrossHairs(int16_t x, int16_t y, uint16_t color) {
  g_display->drawRect(156+x, 23+y, 5, 5, color);
  g_display->drawLine(158+x, 22+y, 158+x, 28+y, color);
  g_display->drawLine(155+x, 25+y, 161+x, 25+y, color);  
}

#define TRIM_STEP		4
#define TRIM_LOLIMIT	15
#define TRIM_HILIMIT	239

bool BIOS::stickTrim()
{
  uint8_t trimX = g_joyTrimX;
  uint8_t trimY = g_joyTrimY;
  uint8_t oldX = 70; 
  uint8_t oldY = 49; 
  uint8_t x = oldX, y = oldY;
  char buf[7];
  g_display->fillRect(157, 24, 141, 97, WHITE);
  g_display->drawRect(157, 24, 142, 98, BLACK);
  while (1) {
	x = (float)trimX / 255.0 * 140.0;
	y = (float)trimY / 255.0 * 96.0;
	if (x != oldX || y != oldY) {
		drawCrossHairs(oldX, oldY, WHITE);
	    sprintf(buf, "X: %d ", trimX);
		g_display->drawString(M_SELECTED, 159, 26, buf);
	    sprintf(buf, "Y: %d ", trimY);
		g_display->drawString(M_SELECTED, 159, 38, buf);
		drawCrossHairs(x, y, BLACK);
		oldX = x;
		oldY = y;
	}
    while (!g_keyboard->kbhit())
      biosIdle();
    switch (g_keyboard->read()) {
      case LARR:
	    trimX -= TRIM_STEP;
		if (trimX < TRIM_LOLIMIT) trimX = TRIM_LOLIMIT;
		break;
      case RARR:
	    trimX += TRIM_STEP;
		if (trimX > TRIM_HILIMIT) trimX = TRIM_HILIMIT;
		break;
      case UARR:
	    trimY -= TRIM_STEP;
		if (trimY < TRIM_LOLIMIT) trimY = TRIM_LOLIMIT;
		break;
      case DARR:
	    trimY += TRIM_STEP;
		if (trimY > TRIM_HILIMIT) trimY = TRIM_HILIMIT;
		break;
      case RET:
	    g_joyTrimX = trimX;
		g_joyTrimY = trimY;
        drawThumb();
	    return true;
      case ESC:
        drawThumb();
	    return false;
	}
  }
}

void drawSpeedBar(uint8_t x) {
  g_display->drawRoundRect(160,45,134,16,3,BLACK);
  g_display->fillRect(162,47,129,11,WHITE);
  g_display->fillRect(162,47,x,11,BLACK);
}

bool BIOS::stickSpeed()
{
  uint8_t speedX = g_joySpeed;
  uint8_t oldX = 70; 
  uint8_t x = oldX;
  
  char buf[20];
  g_display->fillRect(157, 24, 141, 97, WHITE);
  g_display->drawRect(157, 24, 142, 98, BLACK);
  while (1) {
	x = (float)speedX / 26.0 * 129.0;
	if (x != oldX) {
	    sprintf(buf, "Speed: %d ", speedX);
		g_display->drawString(M_SELECTED, 159, 26, buf);
		drawSpeedBar(x);
		oldX = x;
	}
    while (!g_keyboard->kbhit())
      biosIdle();
    switch (g_keyboard->read()) {
      case LARR:
	    speedX -= 1;
		if (speedX < 1) speedX = 1;
		break;
      case RARR:
	    speedX += 1;
		if (speedX > 25) speedX = 25;
		break;
      case RET:
	    g_joySpeed = speedX;
        drawThumb();
	    return true;
      case ESC:
        drawThumb();
	    return false;
	}
  }
}


//---------------------------------------------------------------------------------------------------
// Memory state image


bool BIOS::saveState() {
  uint8_t fn = g_filemanager->openFile("/state.img");
  if (fn == -1) return false;
  g_filemanager->writeState(fn);
  g_filemanager->closeFile(fn);
  return true;
}

bool BIOS::restoreState() {
  uint8_t fn = g_filemanager->openFile("/state.img");
  if (fn == -1) return false;
  g_filemanager->readState(fn);
  g_filemanager->closeFile(fn);
  return true;
}


//---------------------------------------------------------------------------------------------------
// Disk stuff

// return true if the user selects an image
// sets selectedFile (index; -1 = "nope") and fileDirectory[][] (names of up to BIOS_MAXFILES files)
bool BIOS::SelectDiskImage()
{
  static int8_t sel = 0;
  static int8_t oldSel = 0;
  static int8_t page = 0;
  static int8_t oldPage = 0;
  bool force = true;
  
  while (1) {
//    Serial.print(page);
//    Serial.print(",");
//    Serial.println(force);
	if (page != oldPage || force) {
  	  DrawDiskNames(page, sel, force);
      force = false;
	  oldPage = page;
	}
	else if (sel != oldSel) {
	  DrawDiskName(oldSel, page, sel);
	  DrawDiskName(sel, page, sel);
	  oldSel = sel;
	}
    while (!g_keyboard->kbhit())
      biosIdle();
    switch (g_keyboard->read()) {
    case DARR:
      sel++;
      sel %= BIOS_MAXFILES + 2;
      break;
    case UARR:
      sel--;
      if (sel < 0) sel = BIOS_MAXFILES + 1;
      break;
    case RET:
      if (sel == 0) {
	    page--;
	    if (page < 0) page = 0;
	    //	else sel = BIOS_MAXFILES + 1;
      }
      else if (sel == BIOS_MAXFILES+1) {
	    if (hasNextPage) page++;
	    //sel = 0;
      } else {
	    if (strcmp(fileDirectory[sel-1], "../") == 0) {
	      // Go up a directory (strip a directory name from rootPath)
	      stripDirectory();
	      page = 0;
		  force = true;
	      //sel = 0;
	      continue;
	    } else if (fileDirectory[sel-1][strlen(fileDirectory[sel-1])-1] == '/') {
	      // Descend in to the directory. FIXME: file path length?
	      strcat(rootPath, fileDirectory[sel-1]);
	      sel = oldSel = 0;
	      page = oldPage = 0;
		  force = true;
	      continue;
	    } else {
	      selectedFile = sel - 1;
	      return true;
	    }
      }
      break;
    case ESC:
	  return false;
    }
  }
}

void BIOS::stripDirectory()
{
  rootPath[strlen(rootPath)-1] = '\0'; // remove the last character

  while (rootPath[0] && rootPath[strlen(rootPath)-1] != '/') {
    rootPath[strlen(rootPath)-1] = '\0'; // remove the last character again
  }

  // We're either at the previous directory, or we've nulled out the whole thing.

  if (rootPath[0] == '\0') {
    // Never go beyond this
    strcpy(rootPath, "/");
  }
}

#define FILENAME_LENGTH 40
void BIOS::DrawDiskName(uint8_t index, uint8_t page, int8_t selection)
{
  if (index == 0) {
    if (page == 0) {
      g_display->drawString(selection == 0 ? M_SELECTDISABLED : M_DISABLED, 6, 50, "<Prev>");
    } else {
      g_display->drawString(selection == 0 ? M_SELECTED : M_NORMAL, 6, 50, "<Prev>");
    }
	return;
  }
  if (index <= BIOS_MAXFILES) {
    uint8_t i = index - 1;
    if ((fileCount != -1 && i < fileCount)) {
      char buf[FILENAME_LENGTH];  
  	  strncpy(buf, fileDirectory[i], FILENAME_LENGTH - 1);
	  buf[FILENAME_LENGTH - 1] = '\0';
      g_display->drawString((i == selection-1) ? M_SELECTED : M_NORMAL, 6, 50 + 14 * (i+1), buf);
    } else {
      g_display->drawString((i == selection-1) ? M_SELECTDISABLED : M_DISABLED, 6, 50+14*(i+1), "-");
    }
	return;
  }
  // FIXME: this doesn't accurately say whether or not there *are* more.
  if (!hasNextPage) { //fileCount == BIOS_MAXFILES || fileCount == 0) {
    g_display->drawString((BIOS_MAXFILES+1 == selection) ? M_SELECTDISABLED : M_DISABLED, 6, 50 + 14 * (BIOS_MAXFILES+1), "<Next>");
  } else {
    g_display->drawString(BIOS_MAXFILES+1 == selection ? M_SELECTED : M_NORMAL, 6, 50 + 14 * (BIOS_MAXFILES+1), "<Next>");
  }
}



void BIOS::DrawDiskNames(uint8_t page, int8_t selection, bool force)
{
  //static uint8_t fileCount = 0;
  static int16_t oldPage = -1;
  if (page != oldPage || force) {
	fileCount = g_filemanager->readDir(rootPath, "dsk,nib", fileDirectory, page * BIOS_MAXFILES, BIOS_MAXPATH);
	oldPage = page;
	hasNextPage = (fileCount == 10); //HasNextPage(page);
  }
  ((TeensyDisplay *)g_display)->clrScr(0x0010);
  g_display->drawLine(0,16, 320, 16, WHITE);
  g_display->drawString(M_NORMAL, 2, 2, "BIOS Configuration - pick disk");

/*  if (page == 0) {
    g_display->drawString(selection == 0 ? M_SELECTDISABLED : M_DISABLED, 6, 50, "<Prev>");
  } else {
    g_display->drawString(selection == 0 ? M_SELECTED : M_NORMAL, 6, 50, "<Prev>");
  }
*/
  uint8_t i;
  for (i=0; i<BIOS_MAXFILES+2; i++) {
    DrawDiskName(i, page, selection);// {
    //if (fileCount != -1 && i < fileCount) {
  	  //char buf[FILENAME_LENGTH];
	  //strncpy(buf, fileDirectory[i], FILENAME_LENGTH - 1);
	  //buf[FILENAME_LENGTH - 1] = '\0';
      //g_display->drawString((i == selection-1) ? M_SELECTED : M_NORMAL, 6, 50 + 14 * (i+1), buf);
    //} else {
    //  g_display->drawString((i == selection-1) ? M_SELECTDISABLED : M_DISABLED, 6, 50+14*(i+1), "-");
    //}

  }
/*
  // FIXME: this doesn't accurately say whether or not there *are* more.
  if (!hasNextPage) { //fileCount == BIOS_MAXFILES || fileCount == 0) {
    g_display->drawString((i+1 == selection) ? M_SELECTDISABLED : M_DISABLED, 6, 50 + 14 * (i+1), "<Next>");
  } else {
    g_display->drawString(i+1 == selection ? M_SELECTED : M_NORMAL, 6, 50 + 14 * (i+1), "<Next>");
  }*/
}


uint8_t BIOS::GatherFilenames(uint8_t pageOffset)
{
  uint16_t startNum = 10 * pageOffset;
  uint8_t count = 0; // number we're including in our listing

//  Serial.print(pageOffset);
//  Serial.print(",");
//  Serial.println(rootPath);
  while (1) {
    char fn[BIOS_MAXPATH];
    // FIXME: add po, nib
	
    int16_t idx = g_filemanager->readDir(rootPath, "dsk,nib", fn, startNum + count, BIOS_MAXPATH);

    if (idx == -1) {
      return count;
    }

    idx++;

    strncpy(fileDirectory[count], fn, BIOS_MAXPATH);
    count++;

    if (count >= BIOS_MAXFILES) {
      return count;
    }
  }
}


bool BIOS::HasNextPage(uint8_t pageOffset)
{
  uint16_t startNum = BIOS_MAXFILES * pageOffset;
  //uint8_t count = 0; // number we're including in our listing

  char fn[BIOS_MAXPATH];
  // FIXME: add po, nib
  int16_t idx = g_filemanager->readDir(rootPath, "dsk,nib", fn, startNum + BIOS_MAXFILES, BIOS_MAXPATH);
  
  return (idx != -1);
}


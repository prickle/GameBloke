#include <Arduino.h>
#include <wchar.h>
#include "ff.h"                   // File System
#include "teensy-filemanager.h"
#include <string.h> // strcpy
#include "globals.h"
#include "applemmu.h"
#include "applevm.h"

// FIXME: globals are yucky.
DIR dir;
FILINFO fno;
FIL fil;

static TCHAR *char2tchar( const char *charString, int nn, TCHAR *output)
{
  int ii;
  for (ii=0; ii<nn; ii++) {
    output[ii] = (TCHAR)charString[ii];
    if (!charString[ii])
      break;
  }
  return output;
}

static char * tchar2char( const TCHAR * tcharString, int nn, char * charString)
{   int ii;
  for(ii = 0; ii<nn; ii++)
    { charString[ii] = (char)tcharString[ii];
      if(!charString[ii]) break;
    }
  return charString;
}


TeensyFileManager::TeensyFileManager()
{
  numCached = 0;
}

TeensyFileManager::~TeensyFileManager()
{
}

int8_t TeensyFileManager::openFile(const char *name)
{
  // See if there's a hole to re-use...
  for (int i=0; i<numCached; i++) {
    if (cachedNames[i][0] == '\0') {
      strncpy(cachedNames[i], name, MAXPATH-1);
      cachedNames[i][MAXPATH-1] = '\0'; // safety: ensure string terminator
      fileSeekPositions[i] = 0;
      return i;
    }
  }

  // check for too many open files
  if (numCached >= MAXFILES)
    return -1;


  // No, so we'll add it to the end
  strncpy(cachedNames[numCached], name, MAXPATH-1);
  cachedNames[numCached][MAXPATH-1] = '\0'; // safety: ensure string terminator
  fileSeekPositions[numCached] = 0;

  numCached++;
  return numCached-1;
}

void TeensyFileManager::closeFile(int8_t fd)
{
  // invalid fd provided?
  if (fd < 0 || fd >= numCached)
    return;

  // clear the name
  cachedNames[fd][0] = '\0';
}

const char *TeensyFileManager::fileName(int8_t fd)
{
  if (fd < 0 || fd >= numCached)
    return NULL;

  return cachedNames[fd];
}

bool TeensyFileManager::readState(int8_t fd) {
  AppleMMU *mmu;
  mmu = (AppleMMU*)g_vm->getMMU();

  if (cachedNames[fd][0] == 0)
    return false;

  // open, seek, read, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_READ);
  if (rc) {
    Serial.println("failed to open");
    return false;
  }

  UINT v;

  //Read disk names
  char fileName[MAXPATH];
  if (f_read(&fil, fileName, MAXPATH, &v)) return false;
  if (v != MAXPATH) return false;
  ((AppleVM *)g_vm)->insertDisk(0, fileName, false);
  
  if (f_read(&fil, fileName, MAXPATH, &v)) return false;
  if (v != MAXPATH) return false;
  ((AppleVM *)g_vm)->insertDisk(1, fileName, false);
  

  //Restore registers
  uint8_t regs[7];  
  if (f_read(&fil, regs, 7, &v)) return false;
  if (v != 7) return false;
  g_cpu->pc = (regs[0] << 8) + regs[1];
  g_cpu->sp = regs[2];
  g_cpu->a = regs[3];
  g_cpu->x = regs[4];
  g_cpu->y = regs[5];
  g_cpu->flags = regs[6];
  
  //Read Switches
  if (f_read(&fil, &mmu->switches, 2, &v)) return false;
  if (v != 2) return false;
 
  //Read Memory Switches
  uint8_t mem;
  if (f_read(&fil, &mem, 1, &v)) return false;
  if (v != 1) return false;
  mmu->auxRamRead = (mem & 128) > 0;
  mmu->auxRamWrite = (mem & 64) > 0;
  mmu->bank2 = (mem & 32) > 0;
  mmu->readbsr = (mem & 16) > 0;
  mmu->writebsr = (mem & 8) > 0;
  mmu->altzp = (mem & 4) > 0;
  mmu->intcxrom = (mem & 2) > 0;
  mmu->slot3rom = (mem & 1) > 0;  

  //Restore memory
  for (uint16_t i=0; i<0xC0; i++) {
    for (uint8_t j=0; j<2; j++) {
	  f_read(&fil, mmu->ramPages[i][j], 256, &v);
	  if (v != 256) return false;
    }
  }
  for (uint16_t i=0xC0; i<0x100; i++) {
    for (uint8_t j=0; j<4; j++) {
	  f_read(&fil, mmu->ramPages[i][j], 256, &v);
	  if (v != 256) return false;
	}
  }
  mmu->resetDisplay();
  mmu->updateMemoryPages();
  
  f_close(&fil);
  return (v == 256);
}


bool TeensyFileManager::writeState(int8_t fd){
  AppleMMU *mmu;
  mmu = (AppleMMU*)g_vm->getMMU();

  // open, seek, write, close.
  if (fd < 0 || fd >= numCached)
    return false;

  if (cachedNames[fd][0] == 0)
    return false;

  // open, create/overwrite, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_WRITE | FA_CREATE_ALWAYS);
  if (rc) return false;
  UINT v;

  //Write disk name
  f_write(&fil, ((AppleVM *)g_vm)->disk6->DiskName(0), MAXPATH, &v);
  if (v != MAXPATH) return false;
  f_write(&fil, ((AppleVM *)g_vm)->disk6->DiskName(1), MAXPATH, &v);
  if (v != MAXPATH) return false;
    
  //Write Registers
  uint8_t regs[7] = {(uint8_t)(g_cpu->pc >> 8), (uint8_t)(g_cpu->pc & 0xFF), g_cpu->sp, g_cpu->a, g_cpu->x, g_cpu->y, g_cpu->flags};  
  f_write(&fil, regs, 7, &v);
  if (v != 7) return false;

  //Write Switches
  f_write(&fil, &mmu->switches, 2, &v);
  if (v != 2) return false;
 
  //Write Memory Switches
  uint8_t mem = (mmu->auxRamRead?128:0 | mmu->auxRamWrite?64:0 | mmu->bank2?32:0 | mmu->readbsr?16:0 | mmu->writebsr?8:0 | mmu->altzp?4:0 | mmu->intcxrom?2:0 | mmu->slot3rom?1:0);  
  f_write(&fil, &mem, 1, &v);
  if (v != 1) return false;
 
  //Write Memory
  for (uint16_t i=0; i<0xC0; i++) {
    for (uint8_t j=0; j<2; j++) {
	  f_write(&fil, mmu->ramPages[i][j], 256, &v);
	  if (v != 256) return false;
    }
  }
  for (uint16_t i=0xC0; i<0x100; i++) {
    for (uint8_t j=0; j<4; j++) {
	  f_write(&fil, mmu->ramPages[i][j], 256, &v);
	  if (v != 256) return false;
	}
  }
  f_close(&fil);
  return (v == 256);

}

int8_t TeensyFileManager::readDir(const char *where, const char *suffix, char fileDirectory[BIOS_MAXFILES][BIOS_MAXPATH+1], int16_t startIdx, uint16_t maxlen)
{
  //  ... open, read, save next name, close, return 10 or less names.
  //	More efficient.

  int16_t idxCount = 1;
  int8_t idx = 0;

  // First entry is always "../"
  if (startIdx == 0) {
      strcpy(fileDirectory[idx++], "../");
  }

  TCHAR buf[MAXPATH];
  char2tchar(where, MAXPATH, buf);
  buf[strlen(where)-1] = '\0'; // this library doesn't want trailing slashes
  FRESULT rc = f_opendir(&dir, buf);
  if (rc) {
    Serial.printf("f_opendir '%s' failed: %d\n", where, rc);
    return -1;
  }

  while (1) {
    rc = f_readdir(&dir, &fno);
    if (rc || !fno.fname[0]) {
      // No more - all done.
      f_closedir(&dir);
      return idx;
    }

    if (fno.fname[0] == '.' || fno.fname[0] == '_' || fno.fname[0] == '~') {
      // skip MAC fork files and any that have been deleted :/
      continue;
    }

    // skip anything that has the wrong suffix
    char fn[MAXPATH];
    tchar2char(fno.fname, MAXPATH, fn);
    if (suffix && !(fno.fattrib & AM_DIR) && strlen(fn) >= 3) {
      const char *fsuff = &fn[strlen(fn)-3];
      if (strstr(suffix, ","))  {
	// multiple suffixes to check
	bool matchesAny = false;
	const char *p = suffix;
	while (p && strlen(p)) {
	  if (!strncasecmp(fsuff, p, 3)) {
	    matchesAny = true;
	    break;
	  }
	  p = strstr(p, ",")+1;
	}
	if (!matchesAny)
	  //continue;
      //} else {
	// one suffix to check
	if (strcasecmp(fsuff, suffix))
	  continue;
      }
    }

    if (idxCount >= startIdx) {
      if (fno.fattrib & AM_DIR) {
		strcat(fn, "/");
      }
      strncpy(fileDirectory[idx++], fn, maxlen);
	  if (idx >= BIOS_MAXFILES) {
        f_closedir(&dir);
        return idx;
	  }
    }

    idxCount++;
  }

  /* NOTREACHED */	
}

// suffix may be comma-separated
int16_t TeensyFileManager::readDir(const char *where, const char *suffix, char *outputFN, int16_t startIdx, uint16_t maxlen)
{
  //  ... open, read, save next name, close, return name. Horribly
  //  inefficient but hopefully won't break the sd layer. And if it
  //  works then we can make this more efficient later.

  // First entry is always "../"
  if (startIdx == 0) {
      strcpy(outputFN, "../");
      return 0;
  }

  int16_t idxCount = 1;
  TCHAR buf[MAXPATH];
  char2tchar(where, MAXPATH, buf);
  buf[strlen(where)-1] = '\0'; // this library doesn't want trailing slashes
  FRESULT rc = f_opendir(&dir, buf);
  if (rc) {
    Serial.printf("f_opendir '%s' failed: %d\n", where, rc);
    return -1;
  }

  while (1) {
    rc = f_readdir(&dir, &fno);
    if (rc || !fno.fname[0]) {
      // No more - all done.
      f_closedir(&dir);
      return -1;
    }

    if (fno.fname[0] == '.' || fno.fname[0] == '_' || fno.fname[0] == '~') {
      // skip MAC fork files and any that have been deleted :/
      continue;
    }

    // skip anything that has the wrong suffix
    char fn[MAXPATH];
    tchar2char(fno.fname, MAXPATH, fn);
    if (suffix && !(fno.fattrib & AM_DIR) && strlen(fn) >= 3) {
      const char *fsuff = &fn[strlen(fn)-3];
      if (strstr(suffix, ","))  {
	// multiple suffixes to check
	bool matchesAny = false;
	const char *p = suffix;
	while (p && strlen(p)) {
	  if (!strncasecmp(fsuff, p, 3)) {
	    matchesAny = true;
	    break;
	  }
	  p = strstr(p, ",")+1;
	}
	if (!matchesAny)
	  //continue;
      //} else {
	// one suffix to check
	if (strcasecmp(fsuff, suffix))
	  continue;
      }
    }

    if (idxCount == startIdx) {
			Serial.println(startIdx);

      if (fno.fattrib & AM_DIR) {
		strcat(fn, "/");
      }
      strncpy(outputFN, fn, maxlen);
      f_closedir(&dir);
      return idxCount;
    }

    idxCount++;
  }

  /* NOTREACHED */
}

void TeensyFileManager::seekBlock(int8_t fd, uint16_t block, bool isNib)
{
  if (fd < 0 || fd >= numCached)
    return;

  fileSeekPositions[fd] = block * (isNib ? 416 : 256);
}


bool TeensyFileManager::readTrack(int8_t fd, uint8_t *toWhere, bool isNib)
{
  if (fd < 0 || fd >= numCached)
    return false;

  if (cachedNames[fd][0] == 0)
    return false;

  // open, seek, read, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_READ);
  if (rc) {
    Serial.println("failed to open");
    return false;
  }

  rc = f_lseek(&fil, fileSeekPositions[fd]);
  if (rc) {
    Serial.println("readTrack: seek failed");
    f_close(&fil);
    return false;
  }
  UINT v;
  f_read(&fil, toWhere, isNib ? 0x1a00 : (256 * 16), &v);
  f_close(&fil);
  return (v == (isNib ? 0x1a00 : (256 * 16)));
}

bool TeensyFileManager::readBlock(int8_t fd, uint8_t *toWhere, bool isNib)
{
  // open, seek, read, close.
  if (fd < 0 || fd >= numCached)
    return false;

  if (cachedNames[fd][0] == 0)
    return false;

  // open, seek, read, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_READ);
  if (rc) {
    Serial.println("failed to open");
    return false;
  }

  rc = f_lseek(&fil, fileSeekPositions[fd]);
  if (rc) {
    Serial.println("readBlock: seek failed");
    f_close(&fil);
    return false;
  }
  UINT v;
  f_read(&fil, toWhere, isNib ? 416 : 256, &v);
  f_close(&fil);
  return (v == (isNib ? 416 : 256));
}

bool TeensyFileManager::readBlocks(int8_t fd, uint8_t *toWhere, uint8_t blocks, bool isNib)
{
  // open, seek, read, close.
  if (fd < 0 || fd >= numCached)
    return false;

  if (cachedNames[fd][0] == 0)
    return false;

  // open, seek, read, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_READ);
  if (rc) {
    Serial.println("failed to open");
    return false;
  }

  rc = f_lseek(&fil, fileSeekPositions[fd]);
  if (rc) {
    Serial.println("readBlock: seek failed");
    f_close(&fil);
    return false;
  }
  UINT v;
  f_read(&fil, toWhere, (isNib ? 416 : 256) * blocks, &v);
  f_close(&fil);
  return (v == ((isNib ? 416 : 256) * blocks));
}

bool TeensyFileManager::writeBlock(int8_t fd, uint8_t *fromWhere, bool isNib)
{
  // open, seek, write, close.
  if (fd < 0 || fd >= numCached)
    return false;

  if (cachedNames[fd][0] == 0)
    return false;

  // can't write just a single block of a nibblized track
  if (isNib)
    return false;

  // open, seek, write, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_WRITE);
  if (rc) return false;
  rc = f_lseek(&fil, fileSeekPositions[fd]);
  UINT v;
  f_write(&fil, fromWhere, 256, &v);
  f_close(&fil);
  return (v == 256);
}

bool TeensyFileManager::writeTrack(int8_t fd, uint8_t *fromWhere, bool isNib)
{
  // open, seek, write, close.
  if (fd < 0 || fd >= numCached)
    return false;

  if (cachedNames[fd][0] == 0)
    return false;

  // open, seek, write, close.
  TCHAR buf[MAXPATH];
  char2tchar(cachedNames[fd], MAXPATH, buf);
  FRESULT rc = f_open(&fil, (TCHAR*) buf, FA_WRITE);
  if (rc) return false;
  rc = f_lseek(&fil, fileSeekPositions[fd]);
  UINT v;
  f_write(&fil, fromWhere, isNib ? 0x1a00 : (256*16), &v);
  f_close(&fil);
  return (v == (isNib ? 0x1a00 : (256*16)));
}


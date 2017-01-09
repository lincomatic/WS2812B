/*
 * Copyright (c) 2017 Sam C. Lin <lincomatic@gmail.com>
 * This file is part of WS2812Remote

 * WS2812Remote is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.

 * WS2812Remote is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with WS2812Remote; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//  Packet Protocol Test

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include "serialib.h"


#include <windows.h>
#define SLEEP(x) Sleep(x)


#include "../WS2812Remote.h"
uint8_t buf[1024];

#define COMM_PORT "\\\\.\\COM3"

void setChkByte(int pktlen)
{
  uint8_t chk = buf[0] ^ buf[2];
  for (int i=3;i < pktlen;i++) {
    chk ^= buf[i];
  }
  buf[1] = chk;
}


int main(int argc,char *argv[])
{
  serialib serial;
  int rc = serial.Open(COMM_PORT,BAUD_RATE);
  if (rc != 1) {
	  fprintf(stderr, "error opening %s: %d", COMM_PORT, rc);
	  return 1;
  }
 
  // set the mode
  int pktlen = 4;
  buf[0] = START_BYTE_PKT;
  buf[2] = OPC_SET_MODE;
  buf[3] = MODE_BOTH; // MODE_BOTH MODE_GLEDIATOR MODE_PKT
  setChkByte(pktlen);
  serial.Write(buf, pktlen);

  // blank the screen
  pktlen = 3;
  buf[0] = START_BYTE_PKT;
  buf[2] = OPC_BLANK;
  setChkByte(pktlen);
  serial.Write(buf, pktlen);
  SLEEP(1000);
  
  // fill screen with 1 color
  pktlen = 6;
  buf[0] = START_BYTE_PKT;
  buf[2] = OPC_FILL;
  buf[3] = 255;
  buf[4] = 255;
  buf[5] = 255;
  setChkByte(pktlen);
  serial.Write(buf, pktlen);
  SLEEP(1000);

  // cycle random color pixels
  PIXEL_GRB *pixBuf = (PIXEL_GRB *)(buf + 3);
  while (1) {
    buf[0] = START_BYTE_PKT;
    buf[2] = OPC_SHOW_FRAME;
    pixBuf[0].r = rand() % 256;
    pixBuf[0].g = rand() % 256;
    pixBuf[0].b = rand() % 256;
    int i;
    for (i=1;i < PIXEL_CNT;i++) {
      pixBuf[i].r = 0;
      pixBuf[i].g = 0;
      pixBuf[i].b = 0;
    }
    PIXEL_GRB p;
    for (int k=0;k < PIXEL_CNT;k++) {
      uint8_t chk = START_BYTE_PKT ^ OPC_SHOW_FRAME;
      p = pixBuf[0];
    
      for (i=1;i < PIXEL_CNT;i++) {
	pixBuf[i-1] = pixBuf[i];
	chk ^= pixBuf[i-1].r;
	chk ^= pixBuf[i-1].g;
	chk ^= pixBuf[i-1].b;
      }
      pixBuf[PIXEL_CNT-1] = p;
      chk ^= pixBuf[PIXEL_CNT-1].r;
      chk ^= pixBuf[PIXEL_CNT-1].g;
      chk ^= pixBuf[PIXEL_CNT-1].b;
      buf[1] = chk;
      serial.Write(buf,sizeof(PIXEL_GRB)*PIXEL_CNT + 3);
      SLEEP(25);
    }
  }
  return 0;
}

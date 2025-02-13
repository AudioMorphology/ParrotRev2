/******************************************************************************

The Camberwell Parrot

Copyright © 2024 Richard R. Goodwin / Audio Morphology Ltd.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the “Software”), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

******************************************************************************/
/**
 * @file pverb.h
 * 
 * Public domain C implementation of the original freeverb, adapted to utilise
 * PSRAM in the Camberwell Parrot Rev 2.0 Hardware
 *
 * Original C++ version written by Jezard at Dreampoint, June 2000
 */
#ifndef PVERB_H
#define PVERB_H

#include <stdlib.h>

#define PV_NUMCOMBS       6   //8
#define PV_NUMALLPASSES   3   //4
#define PV_MUTED          0.0
#define PV_FIXEDGAIN      0.015
#define PV_SCALEWET       3.0
#define PV_SCALEDRY       2.0
#define PV_SCALEDAMP      0.4
#define PV_SCALEROOM      0.28
#define PV_STEREOSPREAD   23
#define PV_OFFSETROOM     0.7
#define PV_INITIALROOM    0.5
#define PV_INITIALDAMP    0.5
#define PV_INITIALWET     (1.0 / PV_SCALEWET)
#define PV_INITIALDRY     0.0
#define PV_INITIALWIDTH   1.0
#define PV_INITIALMODE    0.0
#define PV_INITIALSR      48000   // Is scaled to 48kHz in pv_set_sample_rate
#define PV_FREEZEMODE     0.5

typedef struct {
  float feedback;
  float filterstore;
  float damp1, damp2;
  uint32_t buf_base;    // Base address of circular buffer
  uint32_t bufsize;     // The length of the delay buffer in samples
  uint32_t bufidx;      // current read/write pointer
} pv_Comb;

typedef struct {
  float feedback;
  uint32_t buf_base;    // Base address of circular buffer
  uint32_t bufsize;     // The length of the delay buffer in samples
  uint32_t bufidx;      // current read/write pointer
} pv_Allpass;

typedef struct {
  float mode;
  float gain;
  float roomsize, roomsize1;
  float damp, damp1;
  float wet, wet1, wet2;
  float dry;
  float width;
  pv_Comb combl[PV_NUMCOMBS];
  pv_Comb combr[PV_NUMCOMBS];
  pv_Allpass allpassl[PV_NUMALLPASSES];
  pv_Allpass allpassr[PV_NUMALLPASSES];
} pv_Context;

#include "../parrot.h"


void pv_init(pv_Context *ctx);
void pv_mute(pv_Context *ctx);
void pv_process(pv_Context *ctx, float *buf, int n);
void pv_set_samplerate(pv_Context *ctx, float value);
void pv_set_mode(pv_Context *ctx, float value);
void pv_set_roomsize(pv_Context *ctx, float value);
void pv_set_damp(pv_Context *ctx, float value);
void pv_set_wet(pv_Context *ctx, float value);
void pv_set_dry(pv_Context *ctx, float value);
void pv_set_width(pv_Context *ctx, float value);

#endif

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
 * @file pverb.c
 * 
 * Implementation of the classic freeware freeverb reverb code modified to
 * utilise PSRAM Buffers within the Camberwell Parrot Rev 2.0 Hardware
 * 
 * Original C++ version written by Jezard at Dreampoint, June 2000
 */
#include <stdio.h>
#include "psram_spi.h"
#include <pico/stdlib.h>
#include "pverb.h"


#define undenormalize(n) { if (xabs(n) < 1e-37) { (n) = 0; } }

static inline float xabs(float n) {
  return n < 0 ? -n : n;
}

//static inline void zeroset(void *buf, int n) {
//  while (n--) { ((char*) buf)[n] = 0; }
//}

/**
 * @brief run the passed sample through the referenced AllPass filter
 */
static inline float allpass_process(pv_Allpass *ap, float input) {
    // Read delayed sample
    union uSample ReadSample, WriteSample;
    uint32_t BufPtr = ap->buf_base + ap->bufidx;
    ReadSample.iSample = psram_read32(&psram_spi, BufPtr << 3);
    undenormalize(ReadSample.fSample);
    union uSample output;
    output.fSample = -input + ReadSample.fSample;
    WriteSample.fSample = input + ReadSample.fSample * ap->feedback;
    psram_write32(&psram_spi, (BufPtr << 3),WriteSample.iSample);
    if (++ap->bufidx >= ap->bufsize) {
      ap->bufidx = 0;
    }
  return output.fSample;
}

/**
 * @brief run the passed sample through the referenced comb filter
 */
static inline float comb_process(pv_Comb *cmb, float input) {
  union uSample ReadSample, WriteSample;
  uint32_t BufPtr = cmb->buf_base + cmb->bufidx;
  ReadSample.iSample = psram_read32(&psram_spi, BufPtr << 3);
  undenormalize(ReadSample.fSample);

  cmb->filterstore = ReadSample.fSample * cmb->damp2 + cmb->filterstore * cmb->damp1;
  undenormalize(cmb->filterstore);
  WriteSample.fSample = input + ReadSample.fSample * cmb->feedback;
  psram_write32(&psram_spi, BufPtr << 3,WriteSample.iSample);
  if (++cmb->bufidx >= cmb->bufsize) {
    cmb->bufidx = 0;
  }
  return ReadSample.fSample;
}

/**
 * @brief Set the damping factor within the passed Comb filter
 */
static inline void comb_set_damp(pv_Comb *cmb, float n) {
  cmb->damp1 = n;
  cmb->damp2 = 1.0 - n;
}

/**
 * @brief initialise the pverb instance defined in pverb.h
 */
void pv_init(pv_Context *ctx) {
  // Set the base address for all of the Buffers in PSRAM
  uint32_t base_address = 0x80000;  // Half-way up the available RAM
  uint32_t buffer_length = 0x2000;  // 8,192 samples - Largest delay is 1600 samples, so more than adequate
  for (int i = 0; i < PV_NUMALLPASSES; i++) {
    ctx->allpassl[i].buf_base = base_address;
    base_address += buffer_length;
    ctx->allpassr[i].buf_base = base_address;
    base_address += buffer_length;
  }
  for (int i = 0; i < PV_NUMCOMBS; i++) {
    ctx->combl[i].buf_base = base_address;
    base_address += buffer_length;
    ctx->combr[i].buf_base = base_address;
    base_address += buffer_length;
  }
  sleep_ms(500);
  pv_set_samplerate(ctx, PV_INITIALSR);
  pv_mute(ctx);
  for (int i = 0; i < PV_NUMALLPASSES; i++) {
    ctx->allpassl[i].feedback = 0.5;
    ctx->allpassr[i].feedback = 0.5;
  }
  pv_set_wet(ctx, PV_INITIALWET);
  pv_set_roomsize(ctx, PV_INITIALROOM);
  pv_set_dry(ctx, PV_INITIALDRY);
  pv_set_damp(ctx, PV_INITIALDAMP);
  pv_set_width(ctx, PV_INITIALWIDTH);
}

/**
 * @brief zero out the buffers
 * 
 * freeverb uses a quickzeroset to do this, but
 * we need to manually write zeroes to the PSRAM buffer
 */
void pv_mute(pv_Context *ctx) {
  //printf("pv_mute\n");
  uint32_t writeptr;
  for (int i = 0; i < PV_NUMCOMBS; i++) {
    for (uint32_t j= 0; j < ctx->combl[i].bufsize; j++){
      writeptr = ctx->combl[i].buf_base + j;
      psram_write32(&psram_spi,writeptr << 3,(uint32_t)0);
    }
    for (uint32_t j= 0; j < ctx->combr[i].bufsize; j++){
      psram_write32(&psram_spi,(ctx->combr[i].buf_base+j) << 3,(uint32_t)0);
    }
  }
  for (int i = 0; i < PV_NUMALLPASSES; i++) {
    for (uint32_t j= 0; j < ctx->allpassl[i].bufsize; j++){
      psram_write32(&psram_spi,(ctx->allpassl[i].buf_base+j) << 3,(uint32_t)0);
    }
    for (uint32_t j= 0; j < ctx->allpassr[i].bufsize; j++){
      psram_write32(&psram_spi,(ctx->allpassr[i].buf_base+j) << 3,(uint32_t)0);
    }
  }
}

/**
 * @brief apply updates to from the passed context
 * to individual Comb filters
 */
static void update(pv_Context *ctx) {
  ctx->wet1 = ctx->wet * (ctx->width * 0.5 + 0.5);
  ctx->wet2 = ctx->wet * ((1 - ctx->width) * 0.5);

  if (ctx->mode >= PV_FREEZEMODE) {
    ctx->roomsize1 = 1;
    ctx->damp1 = 0;
    printf("muted!!\n");
    ctx->gain = PV_MUTED;

  } else {
    ctx->roomsize1 = ctx->roomsize;
    ctx->damp1 = ctx->damp;
    ctx->gain = PV_FIXEDGAIN;
  }

  for (int i = 0; i < PV_NUMCOMBS; i++) {
    ctx->combl[i].feedback = ctx->roomsize1;
    ctx->combr[i].feedback = ctx->roomsize1;
    comb_set_damp(&ctx->combl[i], ctx->damp1);
    comb_set_damp(&ctx->combr[i], ctx->damp1);
  }
}

void pv_set_samplerate(pv_Context *ctx, float value) {
  //const int combs[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }; //44100Hz
  const int combs[] = { 1215, 1293, 1390, 1476, 1548, 1623, 1695, 1760 };   //48000Hz
  //const int allpasses[] = { 556, 441, 341, 225 };   //44100Hz
  const int allpasses[] = { 605, 480, 371, 245 };     //48000Hz
  /* init comb buffers */
  for (int i = 0; i < PV_NUMCOMBS; i++) {
    ctx->combl[i].bufsize = combs[i];
    ctx->combr[i].bufsize = (combs[i] + PV_STEREOSPREAD);
  }

  /* init allpass buffers */
  for (int i = 0; i < PV_NUMALLPASSES; i++) {
    ctx->allpassl[i].bufsize = allpasses[i];
    ctx->allpassr[i].bufsize = (allpasses[i] + PV_STEREOSPREAD);
  }
}

/**
 * @brief Set the Mode.
 * 
 * TODO - Not sure about this - if mode > PV_FREEZEMODE (= 0.5)
 * then the delay is muted & roomsize set = 1, though I'm not sure
 * what the purpose of this is?
 */
void pv_set_mode(pv_Context *ctx, float value) {
  ctx->mode = value;
  update(ctx);
}


/**
 * @brief Set the Room Size within the passed context
 */
void pv_set_roomsize(pv_Context *ctx, float value) {
  ctx->roomsize = value * PV_SCALEROOM + PV_OFFSETROOM;
  update(ctx);
}


/**
 * @brief Set the Damping Factor within the passed context
 */
void pv_set_damp(pv_Context *ctx, float value) {
  ctx->damp = value * PV_SCALEDAMP;
  update(ctx);
}


/**
 * @brief Set the Wet level within the passed context
 */
void pv_set_wet(pv_Context *ctx, float value) {
  ctx->wet = value * PV_SCALEWET;
  update(ctx);
}

/**
 * @brief Set the Dry level within the passed context
 */
void pv_set_dry(pv_Context *ctx, float value) {
  ctx->dry = value * PV_SCALEDRY;
}

/**
 * @brief - set Room Width within the passed Context
 */
void pv_set_width(pv_Context *ctx, float value) {
  ctx->width = value;
  update(ctx);
}

/**
 * @brief Process an incoming L-R sample pair
 */
void pv_process(pv_Context *ctx, float *buf, int n) {
  for (int i = 0; i < n; i += 2) {
    float outl = 0;
    float outr = 0;
    float input = (buf[i] + buf[i + 1]) * ctx->gain;

    /* accumulate comb filters in parallel */
    for (int i = 0; i < PV_NUMCOMBS; i++) {
      outl += comb_process(&ctx->combl[i], input);
      //outr += comb_process(&ctx->combr[i], input);
    }

    /* feed through allpasses in series */
    for (int i = 0; i < PV_NUMALLPASSES; i++) {
      outl = allpass_process(&ctx->allpassl[i], outl);
      //outr = allpass_process(&ctx->allpassr[i], outr);
    }

    /* replace buffer with output */
    buf[i  ] = outl * ctx->wet1 + outr * ctx->wet2 + buf[i  ] * ctx->dry;
    buf[i+1] = outl * ctx->wet1 + outr * ctx->wet2 + buf[i+1] * ctx->dry;
    //buf[i+1] = outr * ctx->wet1 + outl * ctx->wet2 + buf[i+1] * ctx->dry;
  }
}

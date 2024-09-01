/// @file tinylor.c
/// @brief Protocol encoding functions implementation.
#ifndef TINYLOR_IMPL_NO_INCLUDE
#include "tinylor.h"
#endif

void lor_set_channel(lor_req_s* req, lor_channel c) {
  c %= 1024;// prevent overflow of max channel index
            // max offset of 64 (6-bits) multiplied by 16-bits per offset
  req->cset.offset = c / 16;
  req->cset.cbits = 1 << (c % 16);
}

void lor_set_channels(lor_req_s* req, lor_channel first,
                      const unsigned short cbits) {
  first %= 1024;                        // prevent overflow of max channel index
  const int off = first % 16;           // get boundary offset of first channel
  req->cset.offset = (first - off) / 16;// set offset to first aligned channel
  req->cset.cbits = cbits << off;       // shift bits to match first channel
}

void lor_set_unit(lor_req_s* req, const lor_unit u) { req->unit = u; }

int lor_set_effect(lor_req_s* req, const lor_effect e,
                   const lor_effect_args_u* args) {
  switch (e) {
    case LOR_SET_INTENSITY:
    case LOR_FADE:
    case LOR_PULSE:
    case LOR_SET_DMX_INTENSITY:
      if (args == NULL) return -1;
      break;
    default:
      break;
  }
  req->effect = e;
  req->args = args != NULL ? *args : (lor_effect_args_u){0};
  return 0;
}

void lor_set_intensity(lor_req_s* req, const lor_intensity i) {
  const lor_effect_args_u args = {.set_intensity = {i}};
  lor_set_effect(req, LOR_SET_INTENSITY, &args);
}

void lor_set_fade(lor_req_s* req, const lor_intensity start,
                  const lor_intensity end, const lor_decisec ds) {
  const lor_effect_args_u args = {.fade = {start, end, ds}};
  lor_set_effect(req, LOR_FADE, &args);
}

/// @brief Determines the compressed format of the channel set for encoding.
/// @param cset The channel set to determine the format of.
/// @return The format byte header of the channel set for protocol encoding.
static lor_channel_format
lor_get_cset_format(const lor_channel_set* const cset) {
  if (cset->offset) return LOR_FMT_MULTIPART;
  const int c = __builtin_popcount(cset->cbits);
  if (!c) return LOR_FMT_UNIT;
  if (c == 1) return LOR_FMT_SINGLE;
  const int low = cset->cbits & 0xFF;
  const int high = cset->cbits >> 8;
  if (low) {
    if (high) return LOR_FMT_16;
    return LOR_FMT_8L;
  }
  return LOR_FMT_8H;
}

/// @brief Encodes a channel set into a buffer.
/// @param b The buffer to write the channel set to.
/// @param cset The channel set to encode.
/// @return The number of bytes written to the buffer.
/// @note Caller is responsible for ensuring buffer is at least 3 bytes in size.
static int lor_encode_cset(unsigned char* const b,
                           const lor_channel_set* const cset) {
  int w = 0;
  const unsigned char low = cset->cbits & 0xFF;
  const unsigned char high = cset->cbits >> 8;
  unsigned char opts = 0;
  if (cset->offset) {
    if (low) {
      opts = high ? 0 : 0x80;// LOR_CHANNELSET_OPT_8H
    } else {
      opts = 0x40;// LOR_CHANNELSET_OPT_8L
    }
  }
  b[w++] = cset->offset | opts;
  if (low) b[w++] = low;
  if (high) b[w++] = high;
  return w;
}

/// @brief Encodes a decisecond value into a 2-byte buffer.
/// @param b The buffer to write the decisecond value to.
/// @param ds The decisecond value to encode.
/// @return The number of bytes written to the buffer.
/// @note Caller is responsible for ensuring buffer is at least 2 bytes in size.
static int lor_encode_decis(unsigned char* b, const lor_decisec ds) {
  const int t0 = ds >> 8;
  const int t1 = ds & 0xFF;
  b[0] = t0 | (!t0 ? 0x80 : (!t1 ? 0x40 : 0));
  b[1] = t1 ? t1 : 1;
  return 2;
}

/// @brief Encodes an effect into a buffer, including any required arguments
///        for the effect type.
/// @param b The buffer to write the effect to.
/// @param e The effect to encode.
/// @param d Optional effect arguments to encode.
/// @return The number of bytes written to the buffer.
/// @note Caller is responsible for ensuring buffer is large enough to hold the
///       encoded effect and arguments.
static int lor_encode_effect(unsigned char* const b, const lor_effect e,
                             const lor_effect_args_u* const d) {
  int w = 0;
  switch (e) {
    case LOR_SET_INTENSITY:
      b[w++] = d->set_intensity.intensity;
      break;
    case LOR_FADE:
      b[w++] = d->fade.start_intensity;
      b[w++] = d->fade.end_intensity;
      w += lor_encode_decis(b, d->fade.deciseconds);
      break;
    case LOR_PULSE:
      b[w++] = d->pulse.deciseconds;
      break;
    case LOR_SET_DMX_INTENSITY:
      b[w++] = d->set_dmx_intensity.output;
      break;
    default:
      break;
  }
  return w;
}

size_t lor_write(unsigned char* b, const size_t bs, const lor_req_s* r,
                 const size_t rs) {
  size_t h = 0;
  for (size_t i = 0; i < rs; i++) {
    size_t w = 0;
    unsigned char t[16] = {0};
    const lor_req_s* const req = &r[i];
    t[w++] = 0;
    t[w++] = req->unit;
    t[w++] = req->effect | lor_get_cset_format(&req->cset);
    w += lor_encode_effect(&t[w], req->effect, &req->args);
    w += lor_encode_cset(&t[w], &req->cset);
    t[w++] = 0;
    if (w > bs) return w;
    __builtin_memcpy(&b[h], t, w);
    h += w;
  }
  return h;
}

lor_intensity lor_get_intensity(const unsigned char b) {
  // scale b from (0,255) to (240,1) which is the LOR intensity range
  static const lor_intensity ceiling = 240;
  return ceiling - (lor_intensity) ((1.0f - ((float) b / 255.0f)) * 239);
}

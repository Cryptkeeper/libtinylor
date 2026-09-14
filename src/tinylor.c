/// @file tinylor.c
/// @brief Protocol encoding functions implementation.
#ifndef TINYLOR_IMPL_NO_INCLUDE
#include "tinylor.h"
#endif

void lor_unit_set_add(lor_unit_set* s, const lor_unit unit) {
  uint64_t* bits = &s->bits[unit / 64];
  *bits |= (uint64_t) 1 << (unit % 64);
}

void lor_unit_set_all(lor_unit_set* s, const lor_unit* u, const size_t us) {
  for (size_t i = 0; i < us; i++) {
    const lor_unit unit = u[i];
    uint64_t* bits = &s->bits[unit / 64];
    *bits |= (uint64_t) 1 << (unit % 64);
  }
}

void lor_unit_set_clear(lor_unit_set* s) {
  s->bits[0] = 0;
  s->bits[1] = 0;
  s->bits[2] = 0;
  s->bits[3] = 0;
}

void lor_set_channel(lor_req_s* req, lor_channel c) {
  c %= 1024;// prevent overflow of max channel index
            // max offset of 64 (6-bits) multiplied by 16-bits per offset
  req->cset.offset = c / 16;
  req->cset.cbits = 1 << (c % 16);
}

void lor_set_channels(lor_req_s* req, lor_channel first, const uint16_t cbits) {
  first %= 1024;                        // prevent overflow of max channel index
  const unsigned int off = first % 16;  // get boundary offset of first channel
  req->cset.offset = (first - off) / 16;// set offset to first aligned channel
  req->cset.cbits = cbits << off;       // shift bits to match first channel
}

void lor_set_unit(lor_req_s* req, const lor_unit u) {
  req->unit = u;
  req->units = NULL;
}

void lor_set_units(lor_req_s* req, lor_unit_set* units) {
  req->unit = LOR_UNIT_ID_LIST;
  req->units = units;
}

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
  req->args = args != NULL ? *args : (lor_effect_args_u) {0};
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

/// @brief Encodes a single-channel set as one byte: the channel's 0-based index
///        with bit 7 set, as expected by the single-channel mode (0x00). Bit 7
///        carries no information for the controller; it only ensures channel
///        index 0 is not encoded as 0x00, the frame delimiter.
/// @param b The buffer to write the channel byte to.
/// @param cset The channel set to encode. Must have a zero offset and exactly
///             one bit set (see lor_get_cset_format, LOR_FMT_SINGLE).
/// @return The number of bytes written to the buffer, always 1.
/// @note Caller is responsible for ensuring buffer is at least 1 byte in size.
static int lor_encode_channel(uint8_t* const b,
                              const lor_channel_set* const cset) {
  b[0] = 0x80 | __builtin_ctz(cset->cbits);
  return 1;
}

/// @brief Encodes a channel set into a buffer.
/// @param b The buffer to write the channel set to.
/// @param cset The channel set to encode.
/// @return The number of bytes written to the buffer.
/// @note Caller is responsible for ensuring buffer is at least 3 bytes in size.
static int lor_encode_cset(uint8_t* const b,
                           const lor_channel_set* const cset) {
  // zero value offset indicates it can fit within the direct addressing mode
  // values above 0, even when single channel, must write as LOR_FMT_MULTIPART.
  if (!cset->offset && __builtin_popcount(cset->cbits) == 1)
    return lor_encode_channel(b, cset);
  int w = 0;
  const uint8_t low = cset->cbits & 0xFF;
  const uint8_t high = cset->cbits >> 8;
  if (cset->offset) {
    uint8_t opts = 0;
    if (low) {
      opts = high ? 0 : 0x80;// LOR_CHANNELSET_OPT_8H
    } else {
      opts = 0x40;// LOR_CHANNELSET_OPT_8L
    }
    b[w++] = cset->offset | opts;
  }
  if (low) b[w++] = low;
  if (high) b[w++] = high;
  return w;
}

/// @brief Encodes a decisecond value.
/// @param b The buffer to write the decisecond value to.
/// @param ds The decisecond value to encode.
/// @return The number of bytes written to the buffer.
static int lor_encode_decis(uint8_t* b, const lor_decisec ds) {
  const int t0 = ds >> 8;  // high
  const int t1 = ds & 0xFF;// low
  b[0] = t0 | (!t0 ? 0x80 : (!t1 ? 0x40 : 0));
  b[1] = t1 ? t1 : 1;
  return 2;
}

/// @brief Encodes a fade effect as an intensity delta relative to the duration
/// of the effect in deciseconds.
/// @param b The buffer to write the fade value to.
/// @return The number of bytes written to the buffer.
/// @note This appears to output values equivalent to the stock configuration.
/// tinylor originally encoded deciseconds as an independent value (i.e. not
/// relative to the intensity values), but this did not visually match the
/// intended behavior.
static int lor_encode_fade_rate(uint8_t* b, const lor_effect_args_u* args) {
  const int dt =
          __builtin_abs(args->fade.start_intensity - args->fade.end_intensity);
  const int rt =
          (int) ((double) dt * 256.0 / ((double) args->fade.deciseconds * 12));
  return lor_encode_decis(b, rt);
}

/// @brief Encodes an effect into a buffer, including any required arguments
///        for the effect type.
/// @param b The buffer to write the effect to.
/// @param e The effect to encode.
/// @param d Optional effect arguments to encode.
/// @return The number of bytes written to the buffer.
/// @note Caller is responsible for ensuring buffer is large enough to hold the
///       encoded effect and arguments.
static int lor_encode_effect(uint8_t* const b, const lor_effect e,
                             const lor_effect_args_u* const d) {
  int w = 0;
  switch (e) {
    case LOR_SET_INTENSITY:
      b[w++] = d->set_intensity.intensity;
      break;
    case LOR_FADE:
      b[w++] = d->fade.start_intensity;
      b[w++] = d->fade.end_intensity;
      w += lor_encode_fade_rate(&b[w], d);
      break;
    case LOR_PULSE:
      w += lor_encode_decis(b, d->pulse.deciseconds);
      break;
    case LOR_SET_DMX_INTENSITY:
      b[w++] = d->set_dmx_intensity.output;
      break;
    default:
      break;
  }
  return w;
}

/// @def LOR_MAX_UNIT_LIST
/// @brief The size of the unit ID space scanned when encoding a unit list, i.e.
///        one bit per possible \p lor_unit value (0-255). The protocol's count
///        byte allows at most 255 entries, but valid unit IDs are limited to
///        [1, 0xF0] so a list can never exceed 240 entries in practice.
#define LOR_MAX_UNIT_LIST 256

size_t lor_write(uint8_t* b, const size_t bs, const lor_req_s* r,
                 const size_t rs, size_t* wb) {
  size_t h = 0;
  // largest command parameters are ~25b, variable unit list up to
  // LOR_MAX_UNIT_LIST, then padded for safety
  uint8_t t[512] = {0};
  for (size_t i = 0; i < rs; i++) {
    size_t w = 0;
    const lor_req_s* const req = &r[i];
    t[w++] = 0;
    t[w++] = req->unit;
    if (req->units != NULL) {
      const size_t n = w++;// count prefix placeholder
      t[n] = 0;
      for (unsigned int i = 0; i < LOR_MAX_UNIT_LIST; i++) {
        if (req->units->bits[i / 64] & ((uint64_t) 1 << (i % 64))) {
          t[w++] = i;
          t[n]++;
        }
      }
    }
    t[w++] = req->effect | lor_get_cset_format(&req->cset);
    w += lor_encode_effect(&t[w], req->effect, &req->args);
    w += lor_encode_cset(&t[w], &req->cset);
    t[w++] = 0;
    if (h + w > bs) {
      if (wb != NULL) *wb += h;
      return i;
    }
    __builtin_memcpy(&b[h], t, w);
    h += w;
  }
  if (wb != NULL) *wb += h;
  return rs;
}

lor_intensity lor_get_intensity(const uint8_t b) {
  const int intensity = (b * 100 + 127) / 255; // scale 0-255 to 0-100
  if (intensity >= 100) return 1;              // clamp if above max intensity
  return (lor_intensity) (228 - intensity * 2);// inverted linear scale
}

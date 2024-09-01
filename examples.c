#include <assert.h>

/// libtinylor is included here relative to the CMake project, your include path
/// would likely instead match the usage example included in the README.
#include "src/tinylor.h"

/// @brief Fades a single channel on a specific unit from full brightness to no
///        brightness over 1 second.
/// @param unit The unit to fade.
/// @param channel The channel to fade.
static void fade_to_black(const lor_unit unit, const lor_channel channel) {
  lor_req_s req = {0};
  lor_set_unit(&req, unit);
  lor_set_channel(&req, channel);
  lor_intensity start = lor_get_intensity(0xFF);// full brightness
  lor_intensity end = lor_get_intensity(0x00);  // no brightness
  lor_set_fade(&req, start, end, 10);           // 1 second fade

  unsigned char b[32] = {0};// write buffer for encoded data
  size_t written = lor_write(b, sizeof(b), &req, 1);
  assert(written <= sizeof(b));// basic check for encoding success

  // send the encoded data in `b` to the device...
}

/// @brief Enables a twinkle effect on alternating channels for a specific unit.
/// @param unit The unit to twinkle.
static void twinkle_alternating_channels(const lor_unit unit) {
  lor_req_s req = {0};
  lor_set_unit(&req, unit);
  lor_set_channels(&req, 0, 0xAA);// alternating channels 0, 2, 4, 6
  lor_set_effect(&req, LOR_TWINKLE, NULL);

  unsigned char b[32] = {0};// write buffer for encoded data
  size_t written = lor_write(b, sizeof(b), &req, 1);
  assert(written <= sizeof(b));// basic check for encoding success

  // send the encoded data in `b` to the device...
}

/// @brief Turns off all channels on a specific unit.
/// @param unit The unit to turn off.
static void turn_off_unit(const lor_unit unit) {
  lor_req_s req = {0};
  lor_set_unit(&req, unit);
  lor_set_effect(&req, LOR_SET_OFF, NULL);

  unsigned char b[32] = {0};// write buffer for encoded data
  size_t written = lor_write(b, sizeof(b), &req, 1);
  assert(written <= sizeof(b));// basic check for encoding success

  // send the encoded data in `b` to the device...
}

/// @brief Sets the intensity of a single channel on a specific unit.
/// @param unit The unit to set the channel intensity on.
/// @param channel The channel to set the intensity of.
/// @param intensity The intensity to set the channel to. 0 is off, 255 is full
///        brightness.
static void set_channel_intensity(const lor_unit unit,
                                  const lor_channel channel,
                                  const unsigned char intensity) {
  lor_req_s req = {0};
  lor_set_unit(&req, unit);
  lor_set_channel(&req, channel);
  lor_set_intensity(&req, lor_get_intensity(intensity));

  unsigned char b[32] = {0};// write buffer for encoded data
  size_t written = lor_write(b, sizeof(b), &req, 1);
  assert(written <= sizeof(b));// basic check for encoding success

  // send the encoded data in `b` to the device...
}

int main(void) {
  fade_to_black(1, 1);              // fade unit 1, channel 1 to black
  twinkle_alternating_channels(2);  // twinkle unit 2 on alternating channels
  turn_off_unit(3);                 // turn off all channels on unit 3
  set_channel_intensity(1, 4, 0x80);// set unit 1, channel 4 to 50% brightness

  return 0;
}

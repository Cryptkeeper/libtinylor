#undef NDEBUG
#include <assert.h>

#include "tinylor.h"

/// @brief Tests the channel format header byte selection.
/// @param cbits 16-bit bitset of channels to encode.
/// @param expected The expected channel format header byte.
static void test_channel_format_header(const unsigned short cbits,
                                       const lor_channel_format expected) {
  lor_req_s req = {0};
  lor_set_channels(&req, 0, cbits);
  lor_set_unit(&req, 1);                  // placeholder unit
  lor_set_effect(&req, LOR_SET_OFF, NULL);// placeholder effect

  unsigned char b[32] = {0};
  lor_write(b, sizeof(b), &req, 1);

  // extract the encoded channel set format byte
  const unsigned char actual = b[2] & 0xF0;
  assert(actual == expected);
}

/// @brief Tests the channel alignment calculation.
/// @param first The first channel to encode.
/// @param cbits 16-bit bitset of channels relative to \p first to encode.
/// @param expected The expected channel alignment result.
static void test_channel_alignment(const lor_channel first,
                                   const unsigned short cbits,
                                   const lor_channel_set expected) {
  lor_req_s req = {0};
  lor_set_channels(&req, first, cbits);

  assert(req.cset.offset == expected.offset);
  assert(req.cset.cbits == expected.cbits);
}

int main(void) {
  test_channel_format_header(0x00FF, LOR_FMT_8L);
  test_channel_format_header(0xFF00, LOR_FMT_8H);
  test_channel_format_header(0x0F0F, LOR_FMT_16);
  test_channel_format_header(0xFFFF, LOR_FMT_16);
  test_channel_format_header(0x0001, LOR_FMT_SINGLE);
  test_channel_format_header(0x0010, LOR_FMT_SINGLE);

  assert(sizeof(LOR_HEARTBEAT_BYTES) == LOR_HEARTBEAT_SIZE);

  test_channel_alignment(0, 0x00FF, (lor_channel_set){0, 0x00FF});
  test_channel_alignment(0, 0xFF00, (lor_channel_set){0, 0xFF00});
  test_channel_alignment(0, 0xFFFF, (lor_channel_set){0, 0xFFFF});

  // first channel at 16(n) will align with a 16-bit boundary and reduce to an offset of n
  test_channel_alignment(16, 0x00FF, (lor_channel_set){1, 0x00FF});
  test_channel_alignment(32, 0xFF00, (lor_channel_set){2, 0xFF00});

  // non-aligned first channel will align with the nearest 16-bit boundary and shift the bits
  test_channel_alignment(4, 0x000F, (lor_channel_set){0, 0x00F0});
  test_channel_alignment(8, 0x00FF, (lor_channel_set){0, 0xFF00});

  return 0;
}

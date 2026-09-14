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

/// @brief Asserts an encoded frame matches the expected bytes exactly, and that
///        the 0x00 frame delimiter does not appear between the two delimiters.
/// @param b The buffer holding the encoded frame.
/// @param written The number of bytes written to \p b.
/// @param expected The expected frame bytes, including both delimiters.
/// @param expected_size The number of bytes in \p expected.
static void assert_frame(const unsigned char* b, const size_t written,
                         const unsigned char* expected,
                         const size_t expected_size) {
  assert(written == expected_size);
  for (size_t i = 0; i < expected_size; i++) assert(b[i] == expected[i]);

  // 0x00 is the frame delimiter and must never appear inside a frame
  for (size_t i = 1; i + 1 < written; i++) assert(b[i] != 0);
}

/// @brief Tests the encoded frame of a channel set request, from the leading
///        0x00 delimiter through the trailing one.
/// @param cbits 16-bit bitset of channels to encode.
/// @param expected The expected frame bytes.
/// @param expected_size The number of bytes in \p expected.
static void test_channel_set_frame(const unsigned short cbits,
                                   const unsigned char* expected,
                                   const size_t expected_size) {
  lor_req_s req = {0};
  lor_set_channels(&req, 0, cbits);
  lor_set_unit(&req, 1);                     // placeholder unit
  lor_set_effect(&req, LOR_SET_LIGHTS, NULL);// placeholder effect

  unsigned char b[32] = {0};
  const size_t written = lor_write(b, sizeof(b), &req, 1);

  assert_frame(b, written, expected, expected_size);
}

/// @brief Tests that effect parameters are encoded before the channel bytes.
/// @param cbits 16-bit bitset of channels to encode.
/// @param intensity The intensity parameter to encode.
/// @param expected The expected frame bytes.
/// @param expected_size The number of bytes in \p expected.
static void test_intensity_frame(const unsigned short cbits,
                                 const lor_intensity intensity,
                                 const unsigned char* expected,
                                 const size_t expected_size) {
  lor_req_s req = {0};
  lor_set_channels(&req, 0, cbits);
  lor_set_unit(&req, 1);// placeholder unit
  lor_set_intensity(&req, intensity);

  unsigned char b[32] = {0};
  const size_t written = lor_write(b, sizeof(b), &req, 1);

  assert_frame(b, written, expected, expected_size);
}

int main(void) {
  test_channel_format_header(0x00FF, LOR_FMT_8L);
  test_channel_format_header(0xFF00, LOR_FMT_8H);
  test_channel_format_header(0x0F0F, LOR_FMT_16);
  test_channel_format_header(0xFFFF, LOR_FMT_16);
  test_channel_format_header(0x0001, LOR_FMT_SINGLE);
  test_channel_format_header(0x0010, LOR_FMT_SINGLE);

  assert(sizeof(LOR_HEARTBEAT_BYTES) == LOR_HEARTBEAT_SIZE);

  test_channel_alignment(0, 0x00FF, (lor_channel_set) {0, 0x00FF});
  test_channel_alignment(0, 0xFF00, (lor_channel_set) {0, 0xFF00});
  test_channel_alignment(0, 0xFFFF, (lor_channel_set) {0, 0xFFFF});

  // first channel at 16(n) will align with a 16-bit boundary and reduce to an offset of n
  test_channel_alignment(16, 0x00FF, (lor_channel_set) {1, 0x00FF});
  test_channel_alignment(32, 0xFF00, (lor_channel_set) {2, 0xFF00});

  // non-aligned first channel will align with the nearest 16-bit boundary and shift the bits
  test_channel_alignment(4, 0x000F, (lor_channel_set) {0, 0x00F0});
  test_channel_alignment(8, 0x00FF, (lor_channel_set) {0, 0xFF00});
  test_channel_alignment(10, 0x000F, (lor_channel_set) {0, 0x3C00});
  test_channel_alignment(36, 0x0F00, (lor_channel_set) {2, 0xF000});
  test_channel_alignment(192, 0x000F, (lor_channel_set) {12, 0x000F});
  test_channel_alignment(193, 0x000F, (lor_channel_set) {12, 0x001E});

  // channel bytes immediately follow the command byte, without an offset byte
  test_channel_set_frame(0x0041,
                         (unsigned char[]) {0x00, 0x01, 0x31, 0x41, 0x00}, 5);
  test_channel_set_frame(0x2100,
                         (unsigned char[]) {0x00, 0x01, 0x21, 0x21, 0x00}, 5);
  test_channel_set_frame(
          0x2041, (unsigned char[]) {0x00, 0x01, 0x11, 0x41, 0x20, 0x00}, 6);
  test_channel_set_frame(
          0xFFFF, (unsigned char[]) {0x00, 0x01, 0x11, 0xFF, 0xFF, 0x00}, 6);
  test_channel_set_frame(0x0000, (unsigned char[]) {0x00, 0x01, 0x41, 0x00}, 4);

  // effect parameters precede the channel bytes
  test_intensity_frame(0x00FF, 0x78,
                       (unsigned char[]) {0x00, 0x01, 0x33, 0x78, 0xFF, 0x00},
                       6);

  return 0;
}

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

/// @brief Tests the single channel byte encoding (0x80 | channel index).
/// @param c The 0-based channel index to encode.
/// @param expected The expected channel byte.
static void test_single_channel_byte(const lor_channel c,
                                     const unsigned char expected) {
  lor_req_s req = {0};
  lor_set_channel(&req, c);
  lor_set_unit(&req, 1);                     // placeholder unit
  lor_set_effect(&req, LOR_SET_LIGHTS, NULL);// placeholder effect

  unsigned char b[32] = {0};
  const size_t written = lor_write(b, sizeof(b), &req, 1);

  const unsigned char frame[] = {0x00, 0x01, 0x01, expected, 0x00};
  assert_frame(b, written, frame, sizeof(frame));
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

/// @brief Tests the unit set bit storage for a single unit.
/// @param unit The unit to add to an otherwise empty set.
static void test_unit_set_add(const lor_unit unit) {
  lor_unit_set s;
  lor_unit_set_clear(&s);
  lor_unit_set_add(&s, unit);

  for (size_t i = 0; i < sizeof(s.bits) / sizeof(s.bits[0]); i++) {
    const uint64_t expected = i == unit / 64 ? (uint64_t) 1 << (unit % 64) : 0;
    assert(s.bits[i] == expected);
  }
}

/// @brief Tests the encoded frame of a request addressed to a set of units,
///        from the leading 0x00 delimiter through the trailing one.
/// @param units The units to include in the set.
/// @param us The number of units in \p units.
/// @param expected The expected frame bytes.
/// @param expected_size The number of bytes in \p expected.
static void test_unit_list_frame(const lor_unit* units, const size_t us,
                                 const unsigned char* expected,
                                 const size_t expected_size) {
  lor_unit_set s;
  lor_unit_set_clear(&s);
  lor_unit_set_all(&s, units, us);

  lor_req_s req = {0};
  lor_set_units(&req, &s);
  lor_set_effect(&req, LOR_SET_LIGHTS, NULL);// no channels, all off (0x41)

  unsigned char b[64] = {0};
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

  // single channel byte is the 0-based channel index with bit 7 set
  test_single_channel_byte(0, 0x80);
  test_single_channel_byte(1, 0x81);
  test_single_channel_byte(4, 0x84);
  test_single_channel_byte(7, 0x87);
  test_single_channel_byte(8, 0x88);// bit lives in the high mask byte
  test_single_channel_byte(13, 0x8D);
  test_single_channel_byte(15, 0x8F);

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
  test_intensity_frame(0x0004, 0x78,
                       (unsigned char[]) {0x00, 0x01, 0x03, 0x78, 0x82, 0x00},
                       6);
  test_intensity_frame(0x00FF, 0x78,
                       (unsigned char[]) {0x00, 0x01, 0x33, 0x78, 0xFF, 0x00},
                       6);

  // unit set bits land in the correct 64-bit word, including bits 32-63
  test_unit_set_add(1);
  test_unit_set_add(31);
  test_unit_set_add(32);
  test_unit_set_add(33);
  test_unit_set_add(63);
  test_unit_set_add(64);
  test_unit_set_add(200);
  test_unit_set_add(255);

  // unit lists contain exactly the units added, in ascending order
  test_unit_list_frame(
          (lor_unit[]) {1, 3}, 2,
          (unsigned char[]) {0x00, 0xF2, 0x02, 0x01, 0x03, 0x41, 0x00}, 7);
  test_unit_list_frame((lor_unit[]) {40, 70, 100, 200}, 4,
                       (unsigned char[]) {0x00, 0xF2, 0x04, 0x28, 0x46, 0x64,
                                          0xC8, 0x41, 0x00},
                       9);
  test_unit_list_frame(
          (lor_unit[]) {240, 192, 191, 128, 127, 64, 63, 32, 31}, 9,
          (unsigned char[]) {0x00, 0xF2, 0x09, 0x1F, 0x20, 0x3F, 0x40, 0x7F,
                             0x80, 0xBF, 0xC0, 0xF0, 0x41, 0x00},
          14);

  return 0;
}

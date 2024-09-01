/// @file tinylor.h
/// @brief A tiny library for generating LOR protocol requests.
#ifndef TINYLOR_H
#define TINYLOR_H

#include <stddef.h>

/// @typedef lor_channel
/// @brief Represents a channel number, which is a unique identifier for a
///        specific light or group of lights. Channels are typically in the
///        range of [1, 1024].
typedef unsigned short lor_channel;

/// @typedef lor_intensity
/// @brief Represents a scaled intensity value used by the LOR protocol.
/// @note The value must be first converted to a scaled intensity value before
///       being passed to the LOR protocol.
typedef unsigned char lor_intensity;

/// @typedef lor_unit
/// @brief Represents a unit number, which is a unique identifier for a piece of
///        LOR hardware. A value of 0xFF is reserved for broadcast messages.
///        Units are typically in the range of [1, 0xF1] (1-241).
typedef unsigned char lor_unit;

/// @typedef lor_decisec
/// @brief Represents a duration in deciseconds, which is 1/10th of a second.
typedef unsigned short lor_decisec;

/// @struct lor_channel_set
/// @brief Represents a grouping of 16 channels, as a bit set, aligned to a
///        16-channel boundary (via a multiplier).
typedef struct lor_channel_set {
  /// @brief The offset of the first channel in the set.
  /// @note The offset is a 6-bit unsigned integer, with a maximum value of 64.
  unsigned char offset;
  /// @brief The 16-bit bitset of channels to apply the effect to relative to
  ///        the offset.
  /// @note The bitset is a 16-bit unsigned integer, with each bit representing
  ///       a channel in the set. The least significant bit represents the
  ///       first channel in the set, and the most significant bit represents
  ///       the 16th channel in the set.
  unsigned short cbits;
} lor_channel_set;

/// @enum lor_effect
/// @brief Represents the various effects that may be applied to a set of
///        channels on a specific unit. Each effect may require additional
///        arguments, which are stored in the args field of the lor_req_s
///        structure. Fields may be set directly, or better yet, by using the
///        provided helper functions which handle potential data validation
///        and conversion that you may not want to do.
typedef enum lor_effect {
  LOR_SET_LIGHTS = 0x01,       ///< Set the lights to full intensity.
  LOR_SET_OFF = 0x02,          ///< Turn the lights off.
  LOR_SET_INTENSITY = 0x03,    ///< Set the lights to a specific intensity.
  LOR_FADE = 0x04,             ///< Fade between two intensities over time.
  LOR_PULSE = 0x05,            ///< Cycle the lights between two intensities.
  LOR_TWINKLE = 0x06,          ///< Twinkle the lights using hardware controls.
  LOR_SHIMMER = 0x07,          ///< Shimmer the lights using hardware controls.
  LOR_SET_DMX_INTENSITY = 0x08,///< Write raw DMX protocol data.
} lor_effect;

/// @enum lor_channel_format
/// @brief Represents the various formats that a channel set may be encoded in
///        for the LOR protocol. The format is used to determine how the channel
///        set is encoded into binary data for transmission. The format is
///        determined by the number of channels in the set and the alignment of
///        the first channel in the set.
typedef enum lor_channel_format {
  LOR_FMT_SINGLE = 0x00,   ///< Single channel.
  LOR_FMT_16 = 0x10,       ///< 16-bit bitset of channels.
  LOR_FMT_8L = 0x20,       ///< 8-bit bitset of channels, low byte.
  LOR_FMT_8H = 0x30,       ///< 8-bit bitset of channels, high byte.
  LOR_FMT_UNIT = 0x40,     ///< Unit number, no channels.
  LOR_FMT_MULTIPART = 0x50,///< Multi-part channel set.
} lor_channel_format;

/// @union lor_effect_data
/// @brief Union of effect argument structures that may be required by assorted
///        effect types: \p LOR_SET_LIGHTS, \p LOR_FADE, \p LOR_PULSE, and
///        \p LOR_SET_DMX_INTENSITY.
typedef union lor_effect_data {
  /// @brief Required effect arguments for LOR_SET_LIGHTS.
  struct {
    /// @brief The intensity to set the lights to.
    lor_intensity intensity;
  } set_intensity;
  /// @brief Required effect arguments for LOR_FADE.
  struct {
    /// @brief The starting intensity.
    lor_intensity start_intensity;
    /// @brief The ending intensity.
    lor_intensity end_intensity;
    /// @brief The duration of the fade in deciseconds.
    lor_decisec deciseconds;
  } fade;
  /// @brief Required effect arguments for LOR_PULSE.
  struct {
    /// @brief Half the full duration of the pulse in deciseconds.
    lor_decisec deciseconds;
  } pulse;
  /// @brief Required effect arguments for LOR_SET_DMX_INTENSITY.
  struct {
    /// @brief The output value to set the DMX channel to (likely brightness).
    unsigned char output;
  } set_dmx_intensity;
} lor_effect_args_u;

/// @def LOR_HEARTBEAT_BYTES
/// @brief The pre-defined binary representation of a LOR heartbeat message.
#define LOR_HEARTBEAT_BYTES ((unsigned char[]){0, 0xFF, 0x81, 0x56, 0})

/// @def LOR_HEARTBEAT_SIZE
/// @brief The length of the LOR heartbeat message \p LOR_HEARTBEAT_BYTES.
#define LOR_HEARTBEAT_SIZE 5

/// @def LOR_HEARTBEAT_DELAY_MS
/// @brief The intended delay in milliseconds between sending LOR heartbeats.
#define LOR_HEARTBEAT_DELAY_MS 500

/// @def LOR_HEARTBEAT_DELAY_NS
/// @brief The intended delay in nanoseconds between sending LOR heartbeats.
#define LOR_HEARTBEAT_DELAY_NS 500000000

/// @struct lor_req
/// @brief Represents a request to apply an effect to a set of channels on a
///        specific unit. The effect may require additional arguments, which are
///        stored in the args field. Fields may be set directly, or better yet,
///        by using the provided helper functions which handle potential data
///        validation/conversion that you may not want to do.
typedef struct lor_req {
  /// @brief The effect type to apply.
  lor_effect effect;
  /// @brief The effect data, if required by the effect type, otherwise zero.
  lor_effect_args_u args;
  /// @brief The channel set to apply the effect to, may be a single channel.
  lor_channel_set cset;
  /// @brief The unit to apply the effect to.
  lor_unit unit;
} lor_req_s;

/// @brief Configures the request to use a single, absolute channel.
/// @param req The request to configure.
/// @param c The single, absolute channel to apply the effect to, less than 1024.
void lor_set_channel(lor_req_s* req, lor_channel c);

/// @brief Configures the request to use a channel set, in the form of a 16-bit
///        bitset of channels, "starting at" the first channel offset used.
/// @note Any bits within the set that (once re-aligned to a 16-bit boundary)
///       exceed the 16-bit window, will be disregarded. Pre-aligned 16-bit
///       bitsets are recommended for best results. You may set the request's
///       cset offset and cbits fields directly.
/// @param req The request to configure.
/// @param first The first channel in the set.
/// @param cbits The 16-bit bitset of channels to apply the effect to.
void lor_set_channels(lor_req_s* req, lor_channel first, unsigned short cbits);

/// @brief Configures the request to be sent to the specified unit. A unit is
///        required for all requests. A magic value of 0xFF may be used to
///        broadcast the request to all units.
/// @param req The request to configure.
/// @param u The unit to send the request to, or 0xFF to broadcast to all units.
void lor_set_unit(lor_req_s* req, lor_unit u);

/// @brief Configures the request to apply the specified effect. The effect may
///        require additional arguments, which are provided in the args field as
///        a union. If provided, the args field is copied into the request at
///        which point the caller may safely discard the original args. Any
///        non-NULL arguments will be copied into the request, even if the effect
///        does not require them. This allows future expansion of usable effects
///        without modifying the behavior of the function.
/// @param req The request to configure.
/// @param e The effect to apply.
/// @param args The effect arguments, required if effect is LOR_SET_INTENSITY,
///             LOR_FADE, LOR_PULSE, or LOR_SET_DMX_INTENSITY. Should likely be
///             NULL for other effect types.
/// @return 0 on success, -1 for invalid arguments.
int lor_set_effect(lor_req_s* req, lor_effect e, const lor_effect_args_u* args);

/// @brief Configures the request to set the intensity of the lights to the
///        provided value.
/// @note Equivalent to calling lor_set_effect with LOR_SET_INTENSITY and effect
///       arguments set to the provided intensity.
/// @param req The request to configure.
/// @param i The intensity to use. This value should likely pass through a
///          intensity conversion function before being passed to this function.
void lor_set_intensity(lor_req_s* req, lor_intensity i);

/// @brief Configures the request to fade from one intensity to another over a
///        specified duration in deciseconds.
/// @note Equivalent to calling lor_set_effect with LOR_FADE and effect
///       arguments set to the provided start and end intensities and duration.
/// @param req The request to configure.
/// @param start The starting intensity.
/// @param end The ending intensity.
/// @param ds The duration of the fade in deciseconds.
void lor_set_fade(lor_req_s* req, lor_intensity start, lor_intensity end,
                  lor_decisec ds);

/// @brief Encodes and writes up to \p rs requests to the provided buffer \p b
///        as binary data. Requests are first encoded into a scratch buffer to
///        ensure the buffer has enough space to hold the encoded data. The
///        function will attempt to write as many requests as possible to the
///        buffer, up to the provided request count.
/// @return If all requests were written, the number of bytes written to the
///         buffer is returned. If the buffer is too small to hold all requests,
///         the number of bytes required to hold at least one additional request
///         is returned.
size_t lor_write(unsigned char* b, size_t bs, const lor_req_s* r, size_t rs);

/// @typedef lor_intensity_fn
/// @brief Represents a function that converts an arbitrary byte value to a
///        a scaled intensity value used by the LOR protocol.
typedef lor_intensity (*lor_intensity_fn)(unsigned char b);

/// @brief Encodes a [0,0xFF] value into a roughly equivalent LOR intensity
///        value (precision is lossy) that is compatible with the protocol.
/// @note This is a default implementation of the lor_intensity_fn type. It
///       operates via a known truth table from protocol documentation. Other
///       or custom implementations may be ideal for your specific use case.
/// @param b The byte value to convert.
/// @return The scaled intensity value.
lor_intensity lor_get_intensity(unsigned char b);

#endif// TINYLOR_H

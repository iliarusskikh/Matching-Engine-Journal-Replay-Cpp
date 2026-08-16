#pragma once

// =============================================================================
// crc32.hpp — CRC-32 (ISO/IEC 3309 / Ethernet polynomial 0xEDB88320).
//
// Integrity only — not cryptographic. Known check: CRC32("123456789") ==
// 0xCBF43926.
// =============================================================================

#include <cstdint>
#include <cstddef>

namespace mejr {

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
  static std::uint32_t table[256];
  static bool ready = false;
  if (!ready) {
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      table[i] = c;
    }
    ready = true;
  }

  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; ++i) {
    crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

}  // namespace mejr

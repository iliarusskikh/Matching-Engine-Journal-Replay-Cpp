#pragma once

// =============================================================================
// journal_record.hpp — On-disk layout: file header + typed records + CRC32.
//
// File header (16 bytes, little-endian):
//   magic[4] = 'M','E','J','R'
//   version  = 1 (u16)
//   flags    = 0 (u16)
//   created_ns (u64) — metadata only; ignored for matching
//
// Record:
//   type:u8 | body (fixed by type) | crc32:u32 over (type‖body)
//
//   type 1 = NEW    body = order_id:u64 side:u8 otype:u8 qty:i64 price:i64  (26 B)
//   type 2 = CANCEL body = order_id:u64                                      (8 B)
// =============================================================================

#include "command.hpp"
#include "crc32.hpp"
#include "lob/types.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace mejr {

inline constexpr char kJournalMagic[4] = {'M', 'E', 'J', 'R'};
inline constexpr std::uint16_t kJournalVersion = 1;

enum class CmdType : std::uint8_t { New = 1, Cancel = 2 };

enum class DecodeStatus {
  Ok,
  NeedMore,           // buffer too short for a full record (or header)
  BadMagic,
  UnsupportedVersion,
  BadType,
  BadChecksum,
};

#pragma pack(push, 1)
struct JournalFileHeader {
  char magic[4];
  std::uint16_t version;
  std::uint16_t flags;
  std::uint64_t created_ns;
};

struct NewBody {
  std::uint64_t order_id;
  std::uint8_t side;    // 0 Buy, 1 Sell
  std::uint8_t otype;   // 0 Limit, 1 Market
  std::int64_t qty;
  std::int64_t price;
};

struct CancelBody {
  std::uint64_t order_id;
};
#pragma pack(pop)

static_assert(sizeof(JournalFileHeader) == 16, "JournalFileHeader must be 16 bytes");
static_assert(sizeof(NewBody) == 26, "NewBody must be 26 bytes");
static_assert(sizeof(CancelBody) == 8, "CancelBody must be 8 bytes");

namespace detail {

inline void store_u16_le(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v & 0xff);
  p[1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
}

inline void store_u32_le(std::uint8_t* p, std::uint32_t v) {
  p[0] = static_cast<std::uint8_t>(v & 0xff);
  p[1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
  p[2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
  p[3] = static_cast<std::uint8_t>((v >> 24) & 0xff);
}

inline void store_u64_le(std::uint8_t* p, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xff);
  }
}

inline void store_i64_le(std::uint8_t* p, std::int64_t v) {
  store_u64_le(p, static_cast<std::uint64_t>(v));
}

inline std::uint16_t load_u16_le(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

inline std::uint32_t load_u32_le(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

inline std::uint64_t load_u64_le(const std::uint8_t* p) {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
  }
  return v;
}

inline std::int64_t load_i64_le(const std::uint8_t* p) {
  return static_cast<std::int64_t>(load_u64_le(p));
}

inline std::size_t body_size(CmdType t) {
  switch (t) {
    case CmdType::New: return sizeof(NewBody);
    case CmdType::Cancel: return sizeof(CancelBody);
  }
  return 0;
}

}  // namespace detail

inline std::vector<std::uint8_t> encode_header(std::uint64_t created_ns = 0, std::uint16_t flags = 0) {
  std::vector<std::uint8_t> out(sizeof(JournalFileHeader));
  out[0] = static_cast<std::uint8_t>(kJournalMagic[0]);
  out[1] = static_cast<std::uint8_t>(kJournalMagic[1]);
  out[2] = static_cast<std::uint8_t>(kJournalMagic[2]);
  out[3] = static_cast<std::uint8_t>(kJournalMagic[3]);
  detail::store_u16_le(out.data() + 4, kJournalVersion);
  detail::store_u16_le(out.data() + 6, flags);
  detail::store_u64_le(out.data() + 8, created_ns);
  return out;
}

inline DecodeStatus decode_header(const std::uint8_t* p, std::size_t n, JournalFileHeader& out) {
  if (n < sizeof(JournalFileHeader)) {
    return DecodeStatus::NeedMore;
  }
  if (p[0] != kJournalMagic[0] || p[1] != kJournalMagic[1] || p[2] != kJournalMagic[2] || p[3] != kJournalMagic[3]) {
    return DecodeStatus::BadMagic;
  }
  out.magic[0] = static_cast<char>(p[0]);
  out.magic[1] = static_cast<char>(p[1]);
  out.magic[2] = static_cast<char>(p[2]);
  out.magic[3] = static_cast<char>(p[3]);
  out.version = detail::load_u16_le(p + 4);
  out.flags = detail::load_u16_le(p + 6);
  out.created_ns = detail::load_u64_le(p + 8);
  if (out.version != kJournalVersion) {
    return DecodeStatus::UnsupportedVersion;
  }
  return DecodeStatus::Ok;
}

// Encode one command to type‖body‖crc32 (little-endian multi-byte fields).
inline std::vector<std::uint8_t> encode_record(const Command& cmd) {
  std::vector<std::uint8_t> buf;

  if (const auto* n = std::get_if<NewCommand>(&cmd)) {
    buf.resize(1 + sizeof(NewBody) + 4);
    buf[0] = static_cast<std::uint8_t>(CmdType::New);
    detail::store_u64_le(buf.data() + 1, n->req.order_id);
    buf[9] = static_cast<std::uint8_t>(n->req.side);
    buf[10] = static_cast<std::uint8_t>(n->req.type);
    detail::store_i64_le(buf.data() + 11, n->req.qty);
    detail::store_i64_le(buf.data() + 19, n->req.price);
  } else {
    const auto& c = std::get<CancelCommand>(cmd);
    buf.resize(1 + sizeof(CancelBody) + 4);
    buf[0] = static_cast<std::uint8_t>(CmdType::Cancel);
    detail::store_u64_le(buf.data() + 1, c.order_id);
  }

  const std::size_t payload = buf.size() - 4;
  const std::uint32_t crc = crc32(buf.data(), payload);
  detail::store_u32_le(buf.data() + payload, crc);
  return buf;
}

// Decode one record. On Ok, sets consumed and out.
inline DecodeStatus decode_record(const std::uint8_t* p, std::size_t n, Command& out, std::size_t& consumed) {
  consumed = 0;
  if (n < 1) {
    return DecodeStatus::NeedMore;
  }

  const auto type = static_cast<CmdType>(p[0]);
  const std::size_t body = detail::body_size(type);
  if (body == 0) {
    return DecodeStatus::BadType;
  }

  const std::size_t total = 1 + body + 4;
  if (n < total) {
    return DecodeStatus::NeedMore;
  }

  const std::uint32_t expect = detail::load_u32_le(p + 1 + body);
  const std::uint32_t got = crc32(p, 1 + body);
  if (expect != got) {
    return DecodeStatus::BadChecksum;
  }

  if (type == CmdType::New) {
    NewCommand nc;
    nc.req.order_id = detail::load_u64_le(p + 1);
    nc.req.side = static_cast<lob::Side>(p[9]);
    nc.req.type = static_cast<lob::OrderType>(p[10]);
    nc.req.qty = detail::load_i64_le(p + 11);
    nc.req.price = detail::load_i64_le(p + 19);
    out = nc;
  } else {
    CancelCommand cc;
    cc.order_id = detail::load_u64_le(p + 1);
    out = cc;
  }

  consumed = total;
  return DecodeStatus::Ok;
}

}  // namespace mejr

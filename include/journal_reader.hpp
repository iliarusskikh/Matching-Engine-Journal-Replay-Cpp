#pragma once

// =============================================================================
// journal_reader.hpp — Sequential read + CRC verify (fail-closed).
// =============================================================================

#include "command.hpp"
#include "journal_record.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mejr {

enum class JournalError {
  Ok,
  IoFailure,
  BadMagic,
  UnsupportedVersion,
  TruncatedRecord,
  BadChecksum,
  BadType,
};

inline const char* to_string(JournalError e) {
  switch (e) {
    case JournalError::Ok: return "Ok";
    case JournalError::IoFailure: return "IoFailure";
    case JournalError::BadMagic: return "BadMagic";
    case JournalError::UnsupportedVersion: return "UnsupportedVersion";
    case JournalError::TruncatedRecord: return "TruncatedRecord";
    case JournalError::BadChecksum: return "BadChecksum";
    case JournalError::BadType: return "BadType";
  }
  return "?";
}

class JournalReader {
public:
  explicit JournalReader(std::filesystem::path path) : path_(std::move(path)) {
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
      throw std::runtime_error("cannot open journal: " + path_.string());
    }
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    if (sz < 0) {
      throw std::runtime_error("cannot size journal: " + path_.string());
    }
    in.seekg(0, std::ios::beg);
    buf_.resize(static_cast<std::size_t>(sz));
    if (!buf_.empty()) {
      in.read(reinterpret_cast<char*>(buf_.data()), static_cast<std::streamsize>(buf_.size()));
      if (!in) {
        throw std::runtime_error("cannot read journal: " + path_.string());
      }
    }

    JournalFileHeader hdr{};
    const DecodeStatus st = decode_header(buf_.data(), buf_.size(), hdr);
    if (st == DecodeStatus::NeedMore) {
      err_ = JournalError::TruncatedRecord;
      return;
    }
    if (st == DecodeStatus::BadMagic) {
      err_ = JournalError::BadMagic;
      return;
    }
    if (st == DecodeStatus::UnsupportedVersion) {
      err_ = JournalError::UnsupportedVersion;
      return;
    }
    pos_ = sizeof(JournalFileHeader);
    err_ = JournalError::Ok;
  }

  // Returns false on clean EOF. On error, ok() is false and error() is set.
  bool next(Command& out) {
    if (err_ != JournalError::Ok) {
      return false;
    }
    if (pos_ == buf_.size()) {
      return false;  // clean end
    }

    std::size_t consumed = 0;
    const DecodeStatus st = decode_record(buf_.data() + pos_, buf_.size() - pos_, out, consumed);

    if (st == DecodeStatus::Ok) {
      pos_ += consumed;
      ++records_;
      return true;
    }
    if (st == DecodeStatus::NeedMore) {
      err_ = JournalError::TruncatedRecord;
    } else if (st == DecodeStatus::BadChecksum) {
      err_ = JournalError::BadChecksum;
    } else if (st == DecodeStatus::BadType) {
      err_ = JournalError::BadType;
    } else {
      err_ = JournalError::IoFailure;
    }
    return false;
  }

  bool ok() const { return err_ == JournalError::Ok; }
  JournalError error() const { return err_; }
  std::uint64_t record_count() const { return records_; }
  const std::filesystem::path& path() const { return path_; }

private:
  std::filesystem::path path_;
  std::vector<std::uint8_t> buf_;
  std::size_t pos_{0};
  std::uint64_t records_{0};
  JournalError err_{JournalError::Ok};
};

}  // namespace mejr

#pragma once

// =============================================================================
// journal_writer.hpp — Append-only journal (POSIX fd + optional fsync).
//
// Policy (v1): call site does match-then-journal; this class only appends.
// Creates/truncates the file, writes the 16-byte header, then records.
// =============================================================================

#include "command.hpp"
#include "journal_record.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace mejr {

class JournalWriter {
public:
  explicit JournalWriter(std::filesystem::path path, bool fsync_each = false) : path_(std::move(path)), fsync_each_(fsync_each) {
    fd_ = ::open(path_.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd_ < 0) {
      throw std::runtime_error("cannot create journal: " + path_.string());
    }
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    write_all(encode_header(static_cast<std::uint64_t>(now), 0)); // encode the header and write it to the file
    if (fsync_each_) {
      sync(); // sync the file to disk
    }
  }

  JournalWriter(const JournalWriter&) = delete;
  JournalWriter& operator=(const JournalWriter&) = delete;

  JournalWriter(JournalWriter&& other) noexcept : path_(std::move(other.path_)),fsync_each_(other.fsync_each_),
        fd_(other.fd_), records_(other.records_) {
    other.fd_ = -1;
    other.records_ = 0;
  }

  JournalWriter& operator=(JournalWriter&& other) noexcept {
    if (this != &other) {
      close();
      path_ = std::move(other.path_);
      fsync_each_ = other.fsync_each_;
      fd_ = other.fd_;
      records_ = other.records_;
      other.fd_ = -1;
      other.records_ = 0;
    }
    return *this;
  }

  ~JournalWriter() { close(); }

  void append(const Command& cmd) {
    if (fd_ < 0) {
      throw std::runtime_error("journal writer is closed");
    }
    write_all(encode_record(cmd));
    ++records_;
    if (fsync_each_) {
      sync();
    }
  }

  void sync() {
    if (fd_ < 0) {
      return;
    }
    if (::fsync(fd_) != 0) {
      throw std::runtime_error("fsync failed: " + path_.string());
    }
  }

  void close() {
    if (fd_ < 0) {
      return;
    }
    // Always fsync on close so a normal CLI exit is durable for demos.
    (void)::fsync(fd_);
    ::close(fd_);
    fd_ = -1;
  }

  const std::filesystem::path& path() const { return path_; }
  std::uint64_t record_count() const { return records_; }

private:
  void write_all(const std::vector<std::uint8_t>& bytes) {
    const std::uint8_t* p = bytes.data();
    std::size_t left = bytes.size();
    while (left > 0) {
      const ssize_t n = ::write(fd_, p, left);
      if (n < 0) {
        throw std::runtime_error("write failed: " + path_.string());
      }
      p += static_cast<std::size_t>(n);
      left -= static_cast<std::size_t>(n);
    }
  }

  std::filesystem::path path_;
  bool fsync_each_{false};
  int fd_{-1};
  std::uint64_t records_{0};
};

}  // namespace mejr

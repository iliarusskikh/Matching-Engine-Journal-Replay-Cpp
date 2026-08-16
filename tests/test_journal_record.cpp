// =============================================================================
// test_journal_record.cpp — CRC, encode/decode, writer/reader (Phase 2).
// =============================================================================

#include "crc32.hpp"
#include "journal_reader.hpp"
#include "journal_record.hpp"
#include "journal_writer.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool cond, const std::string& msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << '\n';
    ++g_failures;
  } else {
    std::cout << "ok: " << msg << '\n';
  }
}

void test_crc_vector() {
  std::cout << "\n=== CRC32 known vector ===\n";
  const char* s = "123456789";
  const auto crc =
      mejr::crc32(reinterpret_cast<const std::uint8_t*>(s), 9);
  expect(crc == 0xCBF43926u, "CRC32(\"123456789\") == 0xCBF43926");
}

void test_roundtrip() {
  std::cout << "\n=== record round-trip ===\n";
  mejr::NewCommand nc;
  nc.req = {42, lob::Side::Buy, lob::OrderType::Limit, 100, 10};
  const auto bytes = mejr::encode_record(nc);

  mejr::Command out;
  std::size_t consumed = 0;
  const auto st = mejr::decode_record(bytes.data(), bytes.size(), out, consumed);
  expect(st == mejr::DecodeStatus::Ok, "decode Ok");
  expect(consumed == bytes.size(), "consumed full buffer");
  const auto* got = std::get_if<mejr::NewCommand>(&out);
  expect(got != nullptr, "decoded NEW");
  if (got) {
    expect(got->req.order_id == 42 && got->req.price == 100 && got->req.qty == 10,
           "NEW fields match");
  }

  mejr::CancelCommand cc{7};
  const auto cbytes = mejr::encode_record(cc);
  mejr::Command cout_cmd;
  expect(mejr::decode_record(cbytes.data(), cbytes.size(), cout_cmd, consumed) ==
             mejr::DecodeStatus::Ok,
         "CANCEL decode Ok");
  expect(std::get<mejr::CancelCommand>(cout_cmd).order_id == 7, "CANCEL id");
}

void test_bad_checksum() {
  std::cout << "\n=== bad checksum ===\n";
  mejr::NewCommand nc;
  nc.req = {1, lob::Side::Sell, lob::OrderType::Market, 0, 5};
  auto bytes = mejr::encode_record(nc);
  bytes[5] ^= 0x01;  // flip a body byte
  mejr::Command out;
  std::size_t consumed = 0;
  expect(mejr::decode_record(bytes.data(), bytes.size(), out, consumed) ==
             mejr::DecodeStatus::BadChecksum,
         "flipped byte → BadChecksum");
}

void test_writer_reader(const std::filesystem::path& tmp) {
  std::cout << "\n=== writer → reader ===\n";
  const auto path = tmp / "phase2.jnl";

  {
    mejr::JournalWriter w(path, /*fsync_each=*/false);
    mejr::NewCommand a;
    a.req = {1, lob::Side::Buy, lob::OrderType::Limit, 100, 10};
    mejr::NewCommand b;
    b.req = {2, lob::Side::Sell, lob::OrderType::Limit, 100, 10};
    w.append(a);
    w.append(b);
    expect(w.record_count() == 2, "wrote 2 records");
  }

  mejr::JournalReader r(path);
  expect(r.ok(), "reader header ok");
  mejr::Command c;
  expect(r.next(c) && std::holds_alternative<mejr::NewCommand>(c), "rec1 NEW");
  expect(r.next(c) && std::holds_alternative<mejr::NewCommand>(c), "rec2 NEW");
  expect(!r.next(c), "EOF");
  expect(r.ok(), "clean EOF still ok");
  expect(r.record_count() == 2, "read 2");
}

void test_truncated(const std::filesystem::path& tmp) {
  std::cout << "\n=== truncated tail ===\n";
  const auto path = tmp / "trunc.jnl";
  {
    mejr::JournalWriter w(path);
    mejr::NewCommand a;
    a.req = {1, lob::Side::Buy, lob::OrderType::Limit, 50, 1};
    w.append(a);
  }

  // Chop last 3 bytes.
  {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg(0, std::ios::end);
    const auto sz = f.tellg();
    f.close();
    std::filesystem::resize_file(path, static_cast<std::uintmax_t>(sz) - 3);
  }

  mejr::JournalReader r(path);
  expect(r.ok(), "header still ok");
  mejr::Command c;
  expect(!r.next(c), "next fails on trunc");
  expect(r.error() == mejr::JournalError::TruncatedRecord, "TruncatedRecord");
}

}  // namespace

int main() {
  const auto tmp = std::filesystem::temp_directory_path() / "mejr_tests";
  std::filesystem::create_directories(tmp);

  test_crc_vector();
  test_roundtrip();
  test_bad_checksum();
  test_writer_reader(tmp);
  test_truncated(tmp);

  if (g_failures) {
    std::cerr << "\n" << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "\nAll Phase 2 checks passed.\n";
  return 0;
}

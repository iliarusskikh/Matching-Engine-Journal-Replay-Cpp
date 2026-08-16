// =============================================================================
// journal_replay.cpp — Read journal → rematch into fresh engine.
//
// Usage:
//   journal_replay <run.jnl> [--quiet]
// Exit 0 on clean replay; non-zero on integrity / I/O errors.
// =============================================================================

#include "apply_command.hpp"
#include "journal_reader.hpp"
#include "lob/event_format.hpp"
#include "lob/matching_engine.hpp"

#include <iostream>
#include <string>

namespace {

void usage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " <run.jnl> [--quiet]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  bool quiet = false;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--quiet") {
      quiet = true;
    } else if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    } else if (path.empty() && a[0] != '-') {
      path = a;
    } else {
      std::cerr << "unknown arg: " << a << '\n';
      usage(argv[0]);
      return 2;
    }
  }

  if (path.empty()) {
    usage(argv[0]);
    return 2;
  }

  try {
    mejr::JournalReader reader(path);
    if (!reader.ok()) {
      std::cerr << "error: bad journal header: " << mejr::to_string(reader.error())
                << '\n';
      return 1;
    }

    lob::MatchingEngine engine;
    mejr::Command cmd;
    while (reader.next(cmd)) {
      const auto events = mejr::apply_command(engine, cmd);
      if (!quiet) {
        for (const auto& ev : events) {
          std::cout << lob::format_event(ev) << '\n';
        }
      }
    }

    if (!reader.ok()) {
      std::cerr << "error: " << mejr::to_string(reader.error()) << " after "
                << reader.record_count() << " good record(s)\n";
      return 1;
    }

    if (!quiet) {
      std::cerr << "replay: applied " << reader.record_count() << " record(s)\n";
      const auto bids = engine.bids(1);
      const auto asks = engine.asks(1);
      if (!bids.empty()) {
        std::cerr << "best bid " << bids[0].total_qty << "@" << bids[0].price << '\n';
      } else {
        std::cerr << "best bid (empty)\n";
      }
      if (!asks.empty()) {
        std::cerr << "best ask " << asks[0].total_qty << "@" << asks[0].price << '\n';
      } else {
        std::cerr << "best ask (empty)\n";
      }
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << '\n';
    return 1;
  }
}

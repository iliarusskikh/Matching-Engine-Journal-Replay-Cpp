// =============================================================================
// journal_run.cpp — Load scenario → match + append command journal.
//
// Usage:
//   journal_run <scenario.txt> -o <out.jnl> [--fsync-each] [--quiet]
// =============================================================================

#include "apply_command.hpp"
#include "journal_writer.hpp"
#include "lob/event_format.hpp"
#include "lob/matching_engine.hpp"
#include "scenario_loader.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <scenario.txt> -o <out.jnl> [--fsync-each] [--quiet]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string scenario;
  std::string out_path;
  bool fsync_each = false;
  bool quiet = false;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) {
      out_path = argv[++i];
    } else if (a == "--fsync-each") {
      fsync_each = true;
    } else if (a == "--quiet") {
      quiet = true;
    } else if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    } else if (scenario.empty() && a[0] != '-') {
      scenario = a;
    } else {
      std::cerr << "unknown arg: " << a << '\n';
      usage(argv[0]);
      return 2;
    }
  }

  if (scenario.empty() || out_path.empty()) {
    usage(argv[0]);
    return 2;
  }

  try {
    const auto cmds = mejr::load_scenario_file(scenario);
    lob::MatchingEngine engine;
    mejr::JournalWriter writer(out_path, fsync_each);

    for (const auto& cmd : cmds) {
      const auto events = mejr::apply_command(engine, cmd); // match the command and get the events
      writer.append(cmd);  // match-then-journal
      if (!quiet) {
        for (const auto& ev : events) {
          std::cout << lob::format_event(ev) << '\n';
        }
      }
    }

    if (!quiet) {
      std::cerr << "journal: wrote " << writer.record_count() << " record(s) to "
                << out_path << '\n';
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << '\n';
    return 1;
  }
}

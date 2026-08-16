// =============================================================================
// test_identity.cpp — Live events == journaled live == replay (Phase 5 core).
// =============================================================================

#include "apply_command.hpp"
#include "journal_reader.hpp"
#include "journal_writer.hpp"
#include "lob/event_format.hpp"
#include "lob/matching_engine.hpp"
#include "scenario_loader.hpp"

#include <filesystem>
#include <iostream>
#include <string>
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

std::vector<std::string> run_collect(const std::vector<mejr::Command>& cmds) {
  lob::MatchingEngine engine;
  std::vector<std::string> lines;
  for (const auto& c : cmds) {
    for (const auto& ev : mejr::apply_command(engine, c)) {
      lines.push_back(lob::format_event(ev));
    }
  }
  return lines;
}

std::vector<std::string> replay_collect(const std::filesystem::path& path) {
  mejr::JournalReader reader(path);
  expect(reader.ok(), "replay header ok");
  lob::MatchingEngine engine;
  std::vector<std::string> lines;
  mejr::Command cmd;
  while (reader.next(cmd)) {
    for (const auto& ev : mejr::apply_command(engine, cmd)) {
      lines.push_back(lob::format_event(ev));
    }
  }
  expect(reader.ok(), "replay clean EOF");
  return lines;
}

void identity_scenario(const std::string& root, const std::string& rel) {
  std::cout << "\n=== identity " << rel << " ===\n";
  const auto cmds = mejr::load_scenario_file(root + "/" + rel);
  const auto live = run_collect(cmds);

  const auto jpath =
      std::filesystem::temp_directory_path() / "mejr_identity.jnl";
  {
    mejr::JournalWriter w(jpath);
    for (const auto& c : cmds) {
      w.append(c);
    }
  }

  // Journaled path: apply while writing (same as journal_run).
  lob::MatchingEngine eng2;
  std::vector<std::string> journaled;
  {
    // Re-write for a faithful journal_run simulation
    mejr::JournalWriter w(jpath);
    for (const auto& c : cmds) {
      for (const auto& ev : mejr::apply_command(eng2, c)) {
        journaled.push_back(lob::format_event(ev));
      }
      w.append(c);
    }
  }

  const auto replayed = replay_collect(jpath);
  expect(live == journaled, "live == journaled-live");
  expect(live == replayed, "live == replay");
}

}  // namespace

int main(int argc, char** argv) {
  const std::string root = (argc > 1) ? argv[1] : "..";
  identity_scenario(root, "scenarios/g2_exact_fill.txt");
  identity_scenario(root, "scenarios/g3_partial_fill.txt");
  identity_scenario(root, "scenarios/g6_cancel.txt");
  identity_scenario(root, "scenarios/demo_walk.txt");

  if (g_failures) {
    std::cerr << "\n" << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "\nAll identity checks passed.\n";
  return 0;
}

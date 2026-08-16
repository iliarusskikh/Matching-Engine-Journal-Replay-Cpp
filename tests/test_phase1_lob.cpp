// =============================================================================
// test_phase1_lob.cpp — Vendored LOB smoke + scenario loader (Phase 1).
// =============================================================================

#include "apply_command.hpp"
#include "lob/event_format.hpp"
#include "lob/matching_engine.hpp"
#include "scenario_loader.hpp"

#include <iostream>
#include <string>
#include <variant>

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

void test_lob_exact_fill() {
  std::cout << "\n=== LOB exact fill ===\n";
  lob::MatchingEngine engine;
  engine.submit({1, lob::Side::Buy, lob::OrderType::Limit, 100, 10});
  auto ev = engine.submit({2, lob::Side::Sell, lob::OrderType::Limit, 100, 10});
  expect(ev.size() == 2, "Accept + Trade");
  expect(engine.bids(1).empty() && engine.asks(1).empty(), "book empty");
}

void test_scenario_g2(const std::string& root) {
  std::cout << "\n=== scenario g2_exact_fill ===\n";
  const auto cmds = mejr::load_scenario_file(root + "/scenarios/g2_exact_fill.txt");
  expect(cmds.size() == 2, "two NEW commands (CHECKs ignored)");

  lob::MatchingEngine engine;
  std::vector<lob::Event> all;
  for (const auto& c : cmds) {
    auto ev = mejr::apply_command(engine, c);
    all.insert(all.end(), ev.begin(), ev.end());
  }
  expect(all.size() == 3, "ACCEPT + ACCEPT+TRADE events across both submits");
  // First submit: Accept only; second: Accept + Trade
  expect(std::holds_alternative<lob::AcceptedEvent>(all[0]), "first ACCEPT");
  expect(std::holds_alternative<lob::TradeEvent>(all[2]), "TRADE last");
  expect(engine.book().is_uncrossed(), "uncrossed");
}

void test_scenario_cancel(const std::string& root) {
  std::cout << "\n=== scenario g6_cancel ===\n";
  const auto cmds = mejr::load_scenario_file(root + "/scenarios/g6_cancel.txt");
  expect(!cmds.empty(), "loaded cancel scenario");

  lob::MatchingEngine engine;
  for (const auto& c : cmds) {
    (void)mejr::apply_command(engine, c);
  }
  expect(engine.bids(1).empty(), "after cancel book empty or as scenario intends");
}

}  // namespace

int main(int argc, char** argv) {
  // Prefer repo-root relative paths; CTest runs from build/ so pass source dir.
  const std::string root = (argc > 1) ? argv[1] : "..";
  test_lob_exact_fill();
  test_scenario_g2(root);
  test_scenario_cancel(root);
  if (g_failures) {
    std::cerr << "\n" << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "\nAll Phase 1 checks passed.\n";
  return 0;
}

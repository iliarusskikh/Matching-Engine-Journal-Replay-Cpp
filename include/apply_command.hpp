#pragma once

// =============================================================================
// apply_command.hpp — Drive MatchingEngine from a host Command.
// Shared by journal_run and journal_replay.
// =============================================================================

#include "command.hpp"
#include "lob/matching_engine.hpp"
#include "lob/types.hpp"

#include <vector>

namespace mejr {

inline std::vector<lob::Event> apply_command(lob::MatchingEngine& engine, const Command& cmd) {
  if (const auto* n = std::get_if<NewCommand>(&cmd)) {
    return engine.submit(n->req); // submit the new order request to the matching engine
  }
  const auto& c = std::get<CancelCommand>(cmd);
  return engine.cancel(c.order_id); // cancel the order with the given order id
}

}  // namespace mejr

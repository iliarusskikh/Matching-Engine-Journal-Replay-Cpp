#pragma once

// =============================================================================
// command.hpp — Host-side journal commands (not on-disk layout).
//
// NEW / CANCEL are what we append to the journal and rematch on replay.
// On-disk bytes live in journal_record.hpp.
// =============================================================================

#include "lob/types.hpp"

#include <cstdint>
#include <variant>

namespace mejr {

struct NewCommand {
  lob::OrderRequest req{};
};

struct CancelCommand {
  std::uint64_t order_id{};
};

using Command = std::variant<NewCommand, CancelCommand>;

}  // namespace mejr

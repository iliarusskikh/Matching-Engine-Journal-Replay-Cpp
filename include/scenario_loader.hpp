#pragma once

// =============================================================================
// scenario_loader.hpp — Parse LOB-style DSL into mejr::Command list.
//
// Supported (commands that hit the journal):
//   NEW <id> <BUY|SELL> <LIMIT|MARKET> <qty> <price>
//   CANCEL <id>
//
// Ignored (for shared scenario files from the LOB repo):
//   SNAPSHOT / CHECK_* / HELP / QUIT / EXIT / blank / # comments
//
// Unlike the gateway loader, CANCEL is kept — journaling needs it.
// Single-symbol v1: no SYM field.
// =============================================================================

#include "command.hpp"
#include "lob/types.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mejr {

namespace detail {

inline std::string upper(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return s;
}

inline lob::Side parse_side(const std::string& s) {
  const std::string u = upper(s);
  if (u == "BUY") return lob::Side::Buy;
  if (u == "SELL") return lob::Side::Sell;
  throw std::runtime_error("bad side: " + s);
}

inline lob::OrderType parse_type(const std::string& s) {
  const std::string u = upper(s);
  if (u == "LIMIT") return lob::OrderType::Limit;
  if (u == "MARKET") return lob::OrderType::Market;
  throw std::runtime_error("bad type: " + s);
}

inline bool is_ignored_cmd(const std::string& cmd) {
  return cmd == "SNAPSHOT" || cmd == "CHECK_BEST_BID" || cmd == "CHECK_BEST_ASK" ||
         cmd == "CHECK_TRADE_COUNT" || cmd == "CHECK_NO_CROSS" || cmd == "HELP" ||
         cmd == "QUIT" || cmd == "EXIT";
}

}  // namespace detail

// Load NEW/CANCEL commands in file order. Throws on open/parse errors.
inline std::vector<Command> load_scenario_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open scenario: " + path);
  }

  std::vector<Command> out;
  std::string line;
  std::size_t line_no = 0;

  while (std::getline(in, line)) {
    ++line_no;
    const auto hash = line.find('#');
    if (hash != std::string::npos) {
      line.resize(hash);
    }

    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd)) {
      continue;
    }
    cmd = detail::upper(cmd);

    if (detail::is_ignored_cmd(cmd)) {
      continue;
    }

    if (cmd == "NEW") {
      std::uint64_t id = 0;
      std::string side_s, type_s;
      std::int64_t qty = 0, price = 0;
      if (!(iss >> id >> side_s >> type_s >> qty >> price)) {
        throw std::runtime_error("NEW parse error at line " + std::to_string(line_no));
      }
      // OrderRequest field order: id, side, type, price, qty
      lob::OrderRequest req{id, detail::parse_side(side_s), detail::parse_type(type_s), price, qty}; // create a new order request
      out.push_back(NewCommand{req}); // add the new order request to the output vector
    } else if (cmd == "CANCEL") {
      std::uint64_t id = 0;
      if (!(iss >> id)) {
        throw std::runtime_error("CANCEL parse error at line " + std::to_string(line_no));
      }
      out.push_back(CancelCommand{id});
    } else {
      throw std::runtime_error("unknown command '" + cmd + "' at line " +
                               std::to_string(line_no));
    }
  }
  return out;
}

}  // namespace mejr

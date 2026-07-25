#include <catch2/catch_test_macros.hpp>
#include <unordered_map>

#include "px/core/ident.h"

namespace px {
namespace {

TEST_CASE("Ident equality") {
  Ident a{"DEEZZ", "K1"};
  Ident b{"DEEZZ", "K1"};
  Ident c{"DEEZZ", "K2"};
  REQUIRE(a == b);
  REQUIRE(!(a == c));
}

TEST_CASE("Ident as unordered_map key") {
  std::unordered_map<Ident, int> m;
  m[Ident{"JFK", "K6"}] = 1;
  m[Ident{"JFK", "K1"}] = 2;

  REQUIRE(m.at(Ident{"JFK", "K6"}) == 1);
  REQUIRE(m.at(Ident{"JFK", "K1"}) == 2);
  REQUIRE(m.size() == 2);
}

TEST_CASE("Ident same ident different region") {
  Ident a{"WILLM", "K6"};
  Ident b{"DEEZZ", "K6"};
  REQUIRE(!(a == b));
}

}  // namespace
}  // namespace px

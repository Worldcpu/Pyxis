// bfdb 往返集成测试：合成小图 + 真实 PMDG 数据。
// 无 navdata/PMDG_navdata.s3db 时仅跑合成测试。

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "graph_builder.h"
#include "px/core/error.h"
#include "bfdb/bfdb_container.h"
#include "bfdb/graph_codec.h"
#include "bfdb/mora_codec.h"
#include "px/module/navdata/nav_data_ir.h"
#include "px/module/navdata/nav_data_loader.h"
#include "px/module/navdata/nav_database.h"

namespace px {
namespace {

const std::string kPmdgPath = "../navdata/PMDG_navdata.s3db";
const std::string kCachePath = "test_roundtrip.pxdb";

bool FileExists(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fclose(f);
  return true;
}

TEST_CASE("bfdb roundtrip: synthetic small graph", "[navdata]") {
  std::vector<RawWaypoint> wps = {
      {"WPT1", "XX", 40.0, -73.0, WaypointKind::kFix},
      {"WPT2", "XX", 41.0, -74.0, WaypointKind::kVor},
      {"WPT3", "XX", 42.0, -75.0, WaypointKind::kNdb},
  };
  std::vector<RawSegment> segs = {
      {"WPT1", "XX", "WPT2", "XX", "V1", AirwayDirection::kBoth,
       AirwayLevel::kBoth, 100, 300},
  };
  std::vector<RawAirport> aps = {
      {"KJFK", "JFK", 40.64, -73.78, 13},
      {"KLAX", "LAX", 33.94, -118.41, 125},
  };

  GraphBuilder builder(wps, segs, aps);
  const auto& g = builder.graph();
  REQUIRE(g.VertexCount() == 5);
  REQUIRE(builder.VertexByAirport("KJFK") == 3);
  REQUIRE(builder.VertexByAirport("KLAX") == 4);

  // bfdb encode
  const auto& coords = g.coords();
  const auto& offsets = g.offsets();
  const auto& edges = g.edges();
  const auto& idents = builder.Idents();
  const auto& names = builder.AirwayNames();
  const auto& kinds = builder.Kinds();
  std::vector<uint8_t> ho(builder.HasOutboundVec().begin(),
                          builder.HasOutboundVec().end());
  std::vector<uint8_t> hi(builder.HasInboundVec().begin(),
                          builder.HasInboundVec().end());

  StringPool pool;
  auto encoded = EncodeGraph(coords, offsets, edges, idents, names, kinds,
                             ho, hi, pool);
  REQUIRE(encoded.has_value());

  MoraGrid mora;
  mora.SetCell(40, -74, 25);
  std::string mora_bytes = EncodeMoraGrid(mora, pool);

  // write bfdb
  auto wr = BfdbContainer::Write(kCachePath, 2607, "0.1.0", "test", ".",
                        encoded.value(), mora_bytes, pool.blob());
  REQUIRE(wr.has_value());
  REQUIRE(FileExists(kCachePath));

  // read back
  auto hdr = BfdbContainer::ReadHeader(kCachePath);
  REQUIRE(hdr.has_value());
  REQUIRE(hdr->cycle == 2607);

  auto graph_bytes = BfdbContainer::ReadGraphSection(kCachePath);
  REQUIRE(graph_bytes.has_value());
  auto decoded = DecodeGraph(graph_bytes.value(), hdr->pool);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->coords.size() == 5);
  REQUIRE(decoded->edges.size() == edges.size());

  for (size_t i = 0; i < decoded->coords.size(); ++i) {
    REQUIRE(std::abs(decoded->coords[i].latitude - coords[i].latitude) < 1e-5);
    REQUIRE(std::abs(decoded->coords[i].longitude - coords[i].longitude) < 1e-5);
    REQUIRE(decoded->idents[i].ident == idents[i].ident);
    REQUIRE(decoded->vert_kinds[i] == kinds[i]);
  }

  // MORA roundtrip
  auto mora_out = BfdbContainer::ReadDetailSection(kCachePath);
  REQUIRE(mora_out.has_value());
  REQUIRE(!mora_out->empty());
  auto mora_decoded = DecodeMoraGrid(mora_out.value());
  REQUIRE(!mora_decoded.Empty());
  REQUIRE(mora_decoded.MoraAt(Coordinate{40.5, -73.5}) == 25);

  // skip remove
}

TEST_CASE("bfdb roundtrip: real PMDG data", "[integration]") {
  if (!FileExists(kPmdgPath)) {
    SKIP("PMDG_navdata.s3db not found");
  }

  auto db_open = NavDatabase::Open(kPmdgPath, "dfd1");
  REQUIRE(db_open.has_value());
  NavDatabase& db = db_open.value();

  const auto& g = db.graph().graph();
  const int v_orig = g.VertexCount();
  const int e_orig = g.EdgeCount();
  REQUIRE(v_orig > 100000);
  REQUIRE(e_orig > 100000);
  REQUIRE(db.cycle() == 2607);

  // Spot-check: verify VOR/NDB present, KJFK airport exists
  bool has_vor = false, has_ndb = false, found_kjfk = false;
  for (int i = 0; i < std::min(v_orig, 5000); ++i) {
    auto k = db.graph().KindOf(i);
    if (k == WaypointKind::kVor) has_vor = true;
    if (k == WaypointKind::kNdb) has_ndb = true;
  }
  REQUIRE(has_vor);
  REQUIRE(has_ndb);
  // Scan last 20000 vertices (airport range)
  for (int i = v_orig - 1; i >= 0 && i >= v_orig - 20000; --i) {
    if (db.graph().IdentOf(i).ident == "KJFK") { found_kjfk = true; break; }
  }
  REQUIRE(found_kjfk);
  REQUIRE(db.graph().VertexByAirport("KJFK") >= 0);

  // Write bfdb
  REQUIRE(db.WriteUnified(kCachePath).has_value());
  REQUIRE(FileExists(kCachePath));

  // Read bfdb and compare
  auto db_cached = NavDatabase::OpenCached(kCachePath);
  REQUIRE(db_cached.has_value());
  auto& cached = db_cached.value();
  const auto& cg = cached.graph().graph();

  REQUIRE(cached.cycle() == db.cycle());
  REQUIRE(cg.VertexCount() == v_orig);
  REQUIRE(cg.EdgeCount() == e_orig);

  // Sample 100 vertices
  int mismatches = 0;
  const int step = std::max(1, v_orig / 100);
  for (int i = 0; i < v_orig; i += step) {
    if (std::abs(g.CoordOf(i).latitude - cg.CoordOf(i).latitude) > 1e-5 ||
        std::abs(g.CoordOf(i).longitude - cg.CoordOf(i).longitude) > 1e-5)
      ++mismatches;
    REQUIRE(db.graph().IdentOf(i).ident == cached.graph().IdentOf(i).ident);
    REQUIRE(db.graph().KindOf(i) == cached.graph().KindOf(i));
  }
  REQUIRE(mismatches == 0);

  // skip remove
}

}  // namespace
}  // namespace px

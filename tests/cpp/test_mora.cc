#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>

#include "px/core/coordinate.h"
#include "px/core/mora_grid.h"
#include "px/core/route_query.h"

#include "mora_constraint.h"

namespace px {
namespace {

// ===================================================================
// MoraGrid 基础测试
// ===================================================================

TEST_CASE("MoraGrid: 空网格无下界") {
  MoraGrid g;
  CHECK(g.Empty());
  CHECK(g.MoraAt(Coordinate{40.0, -74.0}) == 0);
}

TEST_CASE("MoraGrid: SetCell 后 MoraAt 返回正确值") {
  MoraGrid g;
  g.SetCell(40, -75, 120);
  CHECK_FALSE(g.Empty());
  CHECK(g.MoraAt(Coordinate{40.0, -75.0}) == 120);
  CHECK(g.MoraAt(Coordinate{40.7, -74.3}) == 120);
  CHECK(g.MoraAt(Coordinate{41.0, -75.0}) == 0);
}

TEST_CASE("MoraGrid: 负数坐标向负无穷取整") {
  MoraGrid g;
  g.SetCell(-1, -1, 55);
  CHECK(g.MoraAt(Coordinate{-0.5, -0.5}) == 55);
  CHECK(g.MoraAt(Coordinate{0.5, 0.5}) == 0);
}

TEST_CASE("MoraGrid: 越界索引被拒绝") {
  MoraGrid g;
  g.SetCell(90, 0, 300);
  g.SetCell(0, 180, 300);
  CHECK(g.Empty());
  CHECK(g.MoraAt(Coordinate{90.0, 0.0}) == 0);
  CHECK(g.MoraAt(Coordinate{0.0, 180.0}) == 0);
  g.SetCell(89, 179, 210);
  g.SetCell(-90, -180, 220);
  CHECK(g.MoraAt(Coordinate{89.0, 179.0}) == 210);
  CHECK(g.MoraAt(Coordinate{-90.0, -180.0}) == 220);
}

TEST_CASE("MoraGrid: 非有限坐标返回 0") {
  MoraGrid g;
  g.SetCell(0, 0, 99);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  CHECK(g.MoraAt(Coordinate{nan, 0.0}) == 0);
  CHECK(g.MoraAt(Coordinate{0.0, inf}) == 0);
  CHECK(g.MoraAt(Coordinate{1.0e18, 0.0}) == 0);
}

TEST_CASE("MoraGrid: 覆写单元格保持计数一致") {
  MoraGrid g;
  g.SetCell(10, 20, 100);
  CHECK_FALSE(g.Empty());
  g.SetCell(10, 20, 200);
  CHECK(g.MoraAt(Coordinate{10.0, 20.0}) == 200);
  CHECK_FALSE(g.Empty());
  g.SetCell(10, 20, 0);
  CHECK(g.MoraAt(Coordinate{10.0, 20.0}) == 0);
  CHECK(g.Empty());
}

TEST_CASE("MoraGrid: FromCells 拒绝错误大小") {
  std::vector<int16_t> too_small(10, 5);
  const MoraGrid g = MoraGrid::FromCells(std::move(too_small));
  CHECK(g.Empty());
}

TEST_CASE("MoraGrid: FromCells 接受正确大小并重新统计") {
  std::vector<int16_t> cells(
      static_cast<size_t>(MoraGrid::kLatCount) * MoraGrid::kLonCount, 0);
  cells[0] = 150;
  const MoraGrid g = MoraGrid::FromCells(std::move(cells));
  CHECK_FALSE(g.Empty());
  CHECK(g.MoraAt(Coordinate{-90.0, -180.0}) == 150);
  CHECK(g.MoraAt(Coordinate{0.0, 0.0}) == 0);
}

// ===================================================================
// MoraConstraint 测试
// ===================================================================

RouteQuery WithAltitude(int min_fl, int max_fl) {
  RouteQuery q;
  q.altitude = FlRange{min_fl, max_fl};
  return q;
}

TEST_CASE("MoraConstraint: 全零网格放行") {
  MoraGrid g;
  MoraConstraint c(g);
  EdgeContext ctx{GraphEdge{}, Coordinate{40.0, 0.0}, Coordinate{41.0, 0.0}};
  CHECK(c.Evaluate(ctx, WithAltitude(350, 370)).allowed);
}

TEST_CASE("MoraConstraint: 无巡航高度放行") {
  MoraGrid g;
  g.SetCell(40, 0, 400);
  MoraConstraint c(g);
  RouteQuery q;
  EdgeContext ctx{GraphEdge{}, Coordinate{40.0, 0.0}, Coordinate{41.0, 0.0}};
  CHECK(c.Evaluate(ctx, q).allowed);
}

TEST_CASE("MoraConstraint: MORA 在巡航范围内放行") {
  MoraGrid g;
  g.SetCell(40, 0, 400);
  MoraConstraint c(g);
  EdgeContext ctx{GraphEdge{}, Coordinate{40.0, 0.0}, Coordinate{41.0, 0.0}};
  CHECK(c.Evaluate(ctx, WithAltitude(350, 450)).allowed);
}

TEST_CASE("MoraConstraint: 巡航顶端低于 MORA 阻断") {
  MoraGrid g;
  g.SetCell(40, 0, 400);
  MoraConstraint c(g);
  EdgeContext ctx{GraphEdge{}, Coordinate{40.0, 0.0}, Coordinate{41.0, 0.0}};
  CHECK_FALSE(c.Evaluate(ctx, WithAltitude(300, 350)).allowed);
}

TEST_CASE("MoraConstraint: 长边中间高 MORA 阻断") {
  MoraGrid g;
  g.SetCell(0, 2, 500);
  MoraConstraint c(g);
  EdgeContext ctx{GraphEdge{}, Coordinate{0.0, 0.0}, Coordinate{0.0, 5.0}};
  CHECK_FALSE(c.Evaluate(ctx, WithAltitude(350, 450)).allowed);
}

// ===================================================================
// 真实 MORA 数据 (需先运行 tools/extract_mora.py)
// ===================================================================

TEST_CASE("真实 MORA: 从 navdata/mora_grid.bin 加载") {
  const char* path = "navdata/mora_grid.bin";
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    SKIP("navdata/mora_grid.bin 不存在——运行 tools/extract_mora.py 生成");
    return;
  }

  std::vector<int16_t> cells(
      static_cast<size_t>(MoraGrid::kLatCount) * MoraGrid::kLonCount);
  file.read(reinterpret_cast<char*>(cells.data()),
            cells.size() * sizeof(int16_t));
  REQUIRE(file.good());

  MoraGrid g = MoraGrid::FromCells(std::move(cells));
  REQUIRE_FALSE(g.Empty());

  int populated = 0;
  for (int16_t v : g.cells())
    if (v != 0) ++populated;
  double ratio = 100.0 * populated / g.cells().size();
  INFO("真实 MORA 填充率: " << ratio << "%");
  CHECK(ratio > 50.0);

  // 太平洋中部 MORA 应较低
  int16_t pacific = g.MoraAt(Coordinate{0.0, -150.0});
  INFO("太平洋中部 MORA: " << pacific);
  CHECK(pacific <= 100);

  // 喜马拉雅区域 MORA 应较高
  int16_t himalaya = g.MoraAt(Coordinate{28.0, 87.0});
  INFO("喜马拉雅区域 MORA: " << himalaya);
  CHECK(himalaya >= 100);
}

}  // namespace
}  // namespace px

// 导航数据库的 Open/OpenCached/WriteUnified 工厂方法实现。
// 参考：bravofinder/lib/io/nav_database.cc 中对应路径的类似逻辑。

#include "px/module/navdata/nav_database.h"

#include <memory>
#include <string>

#include "graph_builder.h"
#include "px/core/error.h"
#include "px/core/result.h"
#include "bfdb/bfdb_container.h"
#include "bfdb/graph_codec.h"
#include "bfdb/mora_codec.h"
#include "px/module/navdata/nav_data_ir.h"
#include "px/module/navdata/nav_data_loader.h"

namespace px {

NavDatabase::NavDatabase() = default;
NavDatabase::~NavDatabase() = default;
NavDatabase::NavDatabase(NavDatabase&&) noexcept = default;
NavDatabase& NavDatabase::operator=(NavDatabase&&) noexcept = default;

Result<NavDatabase> NavDatabase::Open(const std::string& source_path,
                                       const std::string& loader_name) {
  auto loader_result = MakeLoader(loader_name);
  if (!loader_result.has_value())
    return tl::make_unexpected(loader_result.error());

  auto loader = std::move(loader_result.value());
  auto ir_result = loader->LoadNavData(source_path);
  if (!ir_result.has_value())
    return tl::make_unexpected(ir_result.error());

  NavDataIR ir = std::move(ir_result.value());

  NavDatabase db;
  db.cycle_ = ir.airac_cycle.empty() ? 0
              : static_cast<uint32_t>(std::stoul(ir.airac_cycle));
  db.mora_ = std::move(ir.mora);
  db.loader_ = std::move(loader);
  db.source_path_ = source_path;
  db.builder_ = std::make_unique<GraphBuilder>(ir.waypoints, ir.segments,
                                               ir.airports);
  return db;
}

Result<NavDatabase> NavDatabase::OpenCached(const std::string& pxdb_path) {
  auto header_result = BfdbContainer::ReadHeader(pxdb_path);
  if (!header_result.has_value())
    return tl::make_unexpected(header_result.error());

  auto& hdr = header_result.value();

  // 校验格式版本
  if (hdr.format_version != BfdbContainer::kFormatVersion)
    return tl::make_unexpected(
        Error(ErrorCode::kFormatMismatch,
              "bfdb format version mismatch; expected " +
                  std::to_string(BfdbContainer::kFormatVersion) + ", got " +
                  std::to_string(hdr.format_version)));

  auto graph_bytes_result = BfdbContainer::ReadGraphSection(pxdb_path);
  if (!graph_bytes_result.has_value())
    return tl::make_unexpected(graph_bytes_result.error());

  auto& pool = hdr.pool;
  auto decoded = DecodeGraph(graph_bytes_result.value(), pool);
  if (!decoded.has_value())
    return tl::make_unexpected(decoded.error());

  NavDatabase db;
  db.cycle_ = hdr.cycle;
  // OpenCached 路径没有 loader
  // 将 vector<uint8_t> 转换为 vector<bool>
  std::vector<bool> ho(decoded->has_outbound.begin(), decoded->has_outbound.end());
  std::vector<bool> hi(decoded->has_inbound.begin(), decoded->has_inbound.end());
  db.builder_ = std::make_unique<GraphBuilder>(
      std::move(decoded->coords), std::move(decoded->offsets),
      std::move(decoded->edges), std::move(decoded->idents),
      std::move(decoded->airway_names), std::move(decoded->vert_kinds),
      std::move(ho), std::move(hi));

  // MORA 从 detail 段加载
  auto mora_result = BfdbContainer::ReadDetailSection(pxdb_path);
  if (mora_result.has_value() && !mora_result.value().empty()) {
    db.mora_ = DecodeMoraGrid(mora_result.value());
  }
  // MORA 缺失不致命——留空网格

  return db;
}

Result<uint32_t> NavDatabase::WriteUnified(const std::string& out_path) const {
  if (!loader_)
    return tl::make_unexpected(
        Error(ErrorCode::kInvalidArgument,
              "cannot write cache: database was opened from a cache, "
              "not from a loader"));

  const auto& graph = builder_->graph();
  const auto& idents = builder_->Idents();
  const auto& airway_names = builder_->AirwayNames();
  const auto& kinds = builder_->Kinds();
  const auto& has_outbound = builder_->HasOutboundVec();
  const auto& has_inbound = builder_->HasInboundVec();

  StringPool pool;
  // has_outbound/has_inbound need to be vector<uint8_t> for EncodeGraph
  std::vector<uint8_t> ho(has_outbound.begin(), has_outbound.end());
  std::vector<uint8_t> hi(has_inbound.begin(), has_inbound.end());
  auto graph_result =
      EncodeGraph(graph.coords(), graph.offsets(), graph.edges(),
                  idents, airway_names, kinds, ho, hi, pool);
  if (!graph_result.has_value())
    return tl::make_unexpected(graph_result.error());
  std::string graph_bytes = std::move(graph_result.value());
  std::string mora_bytes = EncodeMoraGrid(mora_, pool);
  std::string detail_bytes = mora_bytes;

  // 版本号硬编码——后续可从 CMake 注入
  std::string version = "0.1.0";
  BfdbContainer::Write(out_path, cycle_, version,
                        loader_->Name(), source_path_, graph_bytes,
                        detail_bytes, pool.blob());

  return 0;
}

}  // namespace px

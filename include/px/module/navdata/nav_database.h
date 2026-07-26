#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "px/core/mora_grid.h"
#include "px/core/result.h"

namespace px {

class GraphBuilder;
class NavDataLoader;

// 顶层导航数据库：持有已加载的航路图和 MORA 数据。
// 参考 bravofinder NavDatabase —— 构造后只读，shared_ptr<const> 跨线程共享。
//
// Phase 5: Open/OpenCached 工厂方法 + 图访问。
// Phase 7: FindRoutes / MsaForAirport 等查询方法。
// Phase 8: 备降场 / 跑道等批量查询。
class NavDatabase {
 public:
  NavDatabase();
  ~NavDatabase();
  NavDatabase(NavDatabase&&) noexcept;
  NavDatabase& operator=(NavDatabase&&) noexcept;

  // 通过 Loader 解析原始导航数据并建图。
  // loader_name 选择数据源 ("dfd1" 等)，记录为 bfdb 缓存的 source_loader 出处。
  static Result<NavDatabase> Open(const std::string& source_path,
                                  const std::string& loader_name = "dfd1");

  // 从预构建的统一 .pxdb 文件加载，跳过全部解析和建图。
  // 格式不兼容或文件损坏时返回 Error。
  static Result<NavDatabase> OpenCached(const std::string& pxdb_path);

  // 将完整数据库序列化到一个统一 .pxdb 文件中。
  // 需要 loader (Open 路径) —— 从缓存打开的数据库不能写入。
  Result<uint32_t> WriteUnified(const std::string& out_path) const;

  // AIRAC 周期 (如 2607)。解析失败或缓存中无此信息时返回 0。
  uint32_t cycle() const { return cycle_; }

  // 对已建好的图和数据的只读访问。
  const GraphBuilder& graph() const { return *builder_; }
  const MoraGrid& mora() const { return mora_; }

 private:
  uint32_t cycle_ = 0;
  std::unique_ptr<GraphBuilder> builder_;
  MoraGrid mora_;
  std::unique_ptr<NavDataLoader> loader_;
  std::string source_path_;
  // 程序缓存等后续 Phase 加入
};

}  // namespace px

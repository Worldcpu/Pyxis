#pragma once

#include <memory>
#include <optional>
#include <string>

#include "px/core/result.h"
#include "px/module/navdata/nav_data_ir.h"

namespace px {

// 导航数据加载器的抽象基类。解析器将特定数据源（PMDG .s3db、Fenix SQLite 等）
// 转换为 NavDataIR（统一中间表示），供 GraphBuilder 和后续流程使用。
//
// Loader 是无状态的，其方法为 const，因此单个实例可并发解析。
// 参考 bravofinder Loader。
class NavDataLoader {
 public:
  virtual ~NavDataLoader() = default;

  // 从 source_path（指向数据文件，而非目录）中解析航路数据集
  // （航点、导航台、航路、机场坐标、MORA 等）到 NavDataIR，
  // 失败时返回 Error。这是轻量路径：不包括 CIFP 终端程序。
  virtual Result<NavDataIR> LoadNavData(const std::string& source_path) const = 0;

  // 按需加载指定机场的 CIFP 终端程序与跑道数据。
  // Phase 6 实现前返回 nullopt。
  virtual std::optional<ProcedureData> LoadProcedure(
      const std::string& source_path, const std::string& icao) const = 0;

  // 加载器的稳定名称，记录为 source_loader 来源信息。
  virtual std::string Name() const = 0;
};

// 按名称创建 Loader 实例。已知名称: "dfd1" (PMDG .s3db)。
// 未知名称返回 Error(kInvalidArgument)。
// 参考 bravofinder loader_registry.h。
Result<std::unique_ptr<NavDataLoader>> MakeLoader(const std::string& name);

}  // namespace px

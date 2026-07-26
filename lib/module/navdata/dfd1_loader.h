#pragma once

#include <optional>
#include <string>

#include "px/core/result.h"
#include "px/module/navdata/nav_data_ir.h"
#include "px/module/navdata/nav_data_loader.h"

namespace px {

// DFD v1.0 SQLite 加载器。解析 PMDG .s3db 文件（"Data File Definition v1.0"
// 格式，与 RealTraffic / SimToolkitPro / PMDG MSFS 737/777 navdb.s3db 字节一致）
// 为 NavDataIR 中间表示。
//
// source_path 为 .s3db 文件的直接路径。
class Dfd1Loader final : public NavDataLoader {
 public:
  Result<NavDataIR> LoadNavData(const std::string& source_path) const override;
  std::optional<ProcedureData> LoadProcedure(
      const std::string& source_path,
      const std::string& icao) const override;
  std::string Name() const override { return "dfd1"; }
};

}  // namespace px

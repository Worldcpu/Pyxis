#pragma once

// bfdb 统一容器。一个 .pxdb 文件包含一个图段、一个可选的 CIFP 段和一个可选
// 的航电详情段，三者共享一个全局字符串池。
//
// 文件布局：
//   [file header] magic "PXDB" (4B) | format_version (U32) | section_count (U32, fixed=3)
//                 | cycle (U32) | program_version (Str) | source_loader (Str)
//                 | data_dir (Str) | pool_len (U32)
//   [section table] 3 entries: (type U32, offset U64, length U64)
//   [global pool] pool_len bytes
//   [graph section] [cifp section] [detail section]  按 section table 顺序
//
// 参考：bravofinder/lib/io/cache/unified_cache.h

#include <cstdint>
#include <string>
#include <vector>

#include "px/core/result.h"

namespace px {

// -------------------------------------------------------------------
// 段类型枚举。
// -------------------------------------------------------------------
enum class BfdbSectionType : uint32_t {
  kGraph = 1,
  kCifp = 2,
  kDetail = 3,
};

// -------------------------------------------------------------------
// ReadHeader 返回的头部信息，包含所有元数据及段表。
// -------------------------------------------------------------------
struct BfdbHeader {
  uint32_t format_version = 0;
  uint32_t cycle = 0;
  std::string program_version;
  std::string source_loader;
  std::string data_dir;

  struct SectionEntry {
    BfdbSectionType type = BfdbSectionType::kGraph;
    uint64_t offset = 0;
    uint64_t length = 0;
  };
  std::vector<SectionEntry> sections;

  // 全局字符串池 blob，供解码器解析引用。
  std::string pool;
};

// -------------------------------------------------------------------
// bfdb 容器：文件格式的构建与读取。
// -------------------------------------------------------------------
class BfdbContainer {
 public:
  // 当前格式版本。磁盘布局变更时递增。
  static constexpr uint32_t kFormatVersion = 1;

  // 固定段数量（graph + cifp + detail），即使某些段缺失。
  static constexpr uint32_t kSectionCount = 3;

  // 魔数
  static constexpr char kMagic[4] = {'P', 'X', 'D', 'B'};

  // -----------------------------------------------------------------
  // 写入
  // -----------------------------------------------------------------

  // 构建 .pxdb 文件。CIFP 段始终以 absent 写入, detail 段可选。
  // graph_bytes 是通过 EncodeGraph 或等效方式产生的图段字节。
  // detail_bytes 是 MORA 等辅助数据 (空 = absent)。
  // pool_blob 是全局字符串池的原始内容（StringPool::blob()）。
  static Result<void> Write(const std::string& path, uint32_t cycle,
                            const std::string& program_version,
                            const std::string& source_loader,
                            const std::string& data_dir,
                            const std::string& graph_bytes,
                            const std::string& detail_bytes,
                            const std::string& pool_blob);

  // -----------------------------------------------------------------
  // 读取
  // -----------------------------------------------------------------

  // 读取容器头部 + 段表 + 全局池，不解码任何段体。
  // 文件缺失返回 kNotFound，魔数错误或截断返回 kCacheCorrupt，
  // 格式版本不匹配返回 kFormatMismatch。
  static Result<BfdbHeader> ReadHeader(const std::string& path);

  // 读取图段原始字节。利用头部的 offset/length 定位。
  static Result<std::string> ReadGraphSection(const std::string& path);
  // 读取 detail 段原始字节。absent 时返回空字符串。
  static Result<std::string> ReadDetailSection(const std::string& path);
};

}  // namespace px

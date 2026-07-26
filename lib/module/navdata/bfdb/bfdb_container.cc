// bfdb 容器文件 I/O 实现。
//
// 参考：bravofinder/lib/io/cache/unified_cache.h（容器布局），
//       bravofinder 中 UnifiedCache::Build / Open 的类似实现。

#include "bfdb_container.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "byte_io.h"
#include "px/core/error.h"
#include "px/core/result.h"

namespace px {

// ===================================================================
// 约定：字符串字段在文件头中以长度前缀形式（U32 len + raw bytes）存储。
// 段表每个条目为 20 字节：type(U32) + offset(U64) + length(U64)。
// ===================================================================
static constexpr std::streamsize kSectionTableEntrySize = 20;  // 4 + 8 + 8

// -------------------------------------------------------------------
// Write
// -------------------------------------------------------------------
Result<void> BfdbContainer::Write(const std::string& path, uint32_t cycle,
                                   const std::string& program_version,
                                   const std::string& source_loader,
                                   const std::string& data_dir,
                                   const std::string& graph_bytes,
                                   const std::string& detail_bytes,
                                   const std::string& pool_blob) {
  std::string out;
  ByteWriter w(out);

  // ---- 文件头 ----
  w.WriteBytes(kMagic, 4);
  w.WriteU32(kFormatVersion);
  w.WriteU32(kSectionCount);
  w.WriteU32(cycle);
  w.WriteString(program_version);
  w.WriteString(source_loader);
  w.WriteString(data_dir);
  w.WriteU32(static_cast<uint32_t>(pool_blob.size()));
  // 文件头结束

  // 计算段位置
  const size_t header_end = w.Size();
  const size_t section_table_size =
      static_cast<size_t>(kSectionCount) * kSectionTableEntrySize;
  const size_t pool_start = header_end + section_table_size;
  const size_t pool_end = pool_start + pool_blob.size();
  const size_t graph_section_offset = pool_end;
  const size_t graph_section_length = graph_bytes.size();
  const size_t detail_section_offset = graph_section_offset + graph_section_length;
  const size_t detail_section_length = detail_bytes.size();

  // ---- 段表 ----
  // Graph（type=1）
  w.WriteU32(static_cast<uint32_t>(BfdbSectionType::kGraph));
  w.WriteU64(static_cast<uint64_t>(graph_section_offset));
  w.WriteU64(static_cast<uint64_t>(graph_section_length));
  // CIFP（type=2）— absent
  w.WriteU32(static_cast<uint32_t>(BfdbSectionType::kCifp));
  w.WriteU64(0);
  w.WriteU64(0);
  // Detail（type=3）
  w.WriteU32(static_cast<uint32_t>(BfdbSectionType::kDetail));
  w.WriteU64(static_cast<uint64_t>(detail_section_offset));
  w.WriteU64(static_cast<uint64_t>(detail_section_length));

  // ---- 全局字符串池 ----
  w.WriteBytes(pool_blob.data(), pool_blob.size());

  // ---- 图段 ----
  w.WriteBytes(graph_bytes.data(), graph_bytes.size());

  // ---- Detail 段 ----
  if (!detail_bytes.empty()) {
    w.WriteBytes(detail_bytes.data(), detail_bytes.size());
  }

  // ---- 写入文件 ----
  std::ofstream f(path, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!f) {
    return tl::make_unexpected(
        Error(ErrorCode::kInternalError, "cannot open for writing: " + path));
  }
  f.write(w.Data(), static_cast<std::streamsize>(w.Size()));
  if (!f) {
    return tl::make_unexpected(
        Error(ErrorCode::kInternalError, "write failed: " + path));
  }
  return Ok();
}

// -------------------------------------------------------------------
// ReadHeader
// -------------------------------------------------------------------
Result<BfdbHeader> BfdbContainer::ReadHeader(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return tl::make_unexpected(
        Error(ErrorCode::kNotFound, "file not found: " + path));
  }

  // 读入整个文件
  std::string data((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
  if (data.empty()) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt, "empty file: " + path));
  }

  ByteReader r(data.data(), data.size());

  // 校验魔数
  char magic[4] = {};
  r.ReadBytes(magic, 4);
  if (!r.Ok() || std::memcmp(magic, kMagic, 4) != 0) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt, "invalid magic in: " + path));
  }

  BfdbHeader hdr;
  hdr.format_version = r.ReadU32();
  const uint32_t section_count = r.ReadU32();
  hdr.cycle = r.ReadU32();
  hdr.program_version = r.ReadString();
  hdr.source_loader = r.ReadString();
  hdr.data_dir = r.ReadString();
  const uint32_t pool_len = r.ReadU32();

  if (!r.Ok() || section_count != kSectionCount) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt,
              "corrupt header or section_count != 3: " + path));
  }

  // 校验格式版本
  if (hdr.format_version != kFormatVersion) {
    return tl::make_unexpected(Error(
        ErrorCode::kFormatMismatch,
        "expected format version " + std::to_string(kFormatVersion) +
            ", got " + std::to_string(hdr.format_version) + ": " + path));
  }

  // 读取段表
  hdr.sections.resize(section_count);
  for (uint32_t i = 0; i < section_count; ++i) {
    const uint32_t type_val = r.ReadU32();
    hdr.sections[i].type = static_cast<BfdbSectionType>(type_val);
    hdr.sections[i].offset = r.ReadU64();
    hdr.sections[i].length = r.ReadU64();
  }

  // 读取全局字符串池
  hdr.pool.resize(pool_len);
  r.ReadBytes(hdr.pool.data(), static_cast<size_t>(pool_len));

  if (!r.Ok()) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt, "truncated file or pool: " + path));
  }

  return Ok(std::move(hdr));
}

// -------------------------------------------------------------------
// ReadGraphSection
// -------------------------------------------------------------------
Result<std::string> BfdbContainer::ReadGraphSection(const std::string& path) {
  // 先读头部获取图段 offset/length
  auto header_result = ReadHeader(path);
  if (!header_result) {
    return tl::make_unexpected(header_result.error());
  }

  const BfdbHeader& hdr = *header_result;

  // 查找图段（type = 1）
  uint64_t graph_offset = 0;
  uint64_t graph_length = 0;
  for (const auto& entry : hdr.sections) {
    if (entry.type == BfdbSectionType::kGraph) {
      graph_offset = entry.offset;
      graph_length = entry.length;
      break;
    }
  }

  if (graph_offset == 0 || graph_length == 0) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt,
              "graph section absent or empty: " + path));
  }

  // 打开文件读取图段
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return tl::make_unexpected(
        Error(ErrorCode::kNotFound,
              "cannot re-open for graph section: " + path));
  }

  std::string section(static_cast<size_t>(graph_length), '\0');
  f.seekg(static_cast<std::streamoff>(graph_offset));
  f.read(section.data(), static_cast<std::streamsize>(graph_length));

  if (!f) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt,
              "cannot read graph section bytes: " + path));
  }

  return Ok(std::move(section));
}



// -------------------------------------------------------------------
// ReadDetailSection
// -------------------------------------------------------------------
Result<std::string> BfdbContainer::ReadDetailSection(const std::string& path) {
  auto header_result = ReadHeader(path);
  if (!header_result) {
    return tl::make_unexpected(header_result.error());
  }

  const BfdbHeader& hdr = *header_result;

  uint64_t detail_offset = 0;
  uint64_t detail_length = 0;
  for (const auto& entry : hdr.sections) {
    if (entry.type == BfdbSectionType::kDetail) {
      detail_offset = entry.offset;
      detail_length = entry.length;
      break;
    }
  }

  if (detail_length == 0) {
    return Ok(std::string{});  // absent = empty
  }

  std::ifstream f(path, std::ios::binary);
  if (!f) return tl::make_unexpected(Error(ErrorCode::kNotFound, "cannot re-open: " + path));

  std::string section(static_cast<size_t>(detail_length), '\0');
  f.seekg(static_cast<std::streamoff>(detail_offset));
  f.read(section.data(), static_cast<std::streamsize>(detail_length));
  if (!f) return tl::make_unexpected(Error(ErrorCode::kCacheCorrupt, "cannot read detail: " + path));

  return Ok(std::move(section));
}
}  // namespace px

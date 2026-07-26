#pragma once

// MORA 网格编解码器。将 MoraGrid 的 180×360 int16 单元格数组序列化为
// 小端 I16 扁平字节，以及反向解码。
//
// 编码格式：64800 个 I16 值（2 字节/单元格 × 64800 = 129600 字节），
// 行主序（纬度优先），每个值为飞行高度层（FL），0 表示未知。
//
// 参考：bravofinder 中 MoraGrid 的序列化（位于 graph_codec.cc 的 MORA 部分）。

#include <cstdint>
#include <string>
#include <vector>

#include "px/core/mora_grid.h"
#include "px/core/result.h"
#include "byte_io.h"

namespace px {

// 将 MoraGrid 编码为扁平 I16 字节。结果字符串长度固定为 129600 字节。
// StringPool 参数供未来扩展，当前未使用。
inline std::string EncodeMoraGrid(const MoraGrid& grid, StringPool& /*pool*/) {
  const auto& cells = grid.cells();
  // 网格单元格数必须符合预期
  const size_t cell_count = static_cast<size_t>(MoraGrid::kLatCount) *
                            MoraGrid::kLonCount;
  if (cells.size() != cell_count) {
    // 返回空串表示编解码无效（调用方应检查）
    return {};
  }

  std::string out;
  ByteWriter w(out);
  w.Reserve(cell_count * sizeof(int16_t));

  for (int16_t cell : cells) {
    w.WriteI16(cell);
  }
  return out;
}

// 从扁平 I16 字节解码 MoraGrid。字节长度必须为 129600 字节。
// 若格式错误则返回空的 MoraGrid（通过 MoraGrid::FromCells 处理）。
inline MoraGrid DecodeMoraGrid(const std::string& bytes) {
  const size_t expected = static_cast<size_t>(MoraGrid::kLatCount) *
                          MoraGrid::kLonCount * sizeof(int16_t);
  if (bytes.size() != expected) {
    return MoraGrid{};
  }

  ByteReader r(bytes.data(), bytes.size());
  std::vector<int16_t> cells(static_cast<size_t>(MoraGrid::kLatCount) *
                             MoraGrid::kLonCount);
  r.ReadI16Span(cells.data(), cells.size());

  if (!r.Ok()) {
    return MoraGrid{};
  }

  return MoraGrid::FromCells(std::move(cells));
}

}  // namespace px

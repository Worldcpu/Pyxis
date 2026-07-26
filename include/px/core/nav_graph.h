#pragma once

#include <cstdint>
#include <vector>

#include "px/core/coordinate.h"
#include "px/core/graph_edge.h"

namespace px {

// 导航航路点上的不可变有向图，以 CSR 格式存储，用于对缓存友好的遍历。
// 顶点为整数索引；每个顶点携带其坐标，供 A* 计算大圆启发式函数。
// 通过 GraphBuilder 构建实例。
class NavGraph {
 public:
  NavGraph() = default;

  int VertexCount() const { return static_cast<int>(coords_.size()); }

  const Coordinate& CoordOf(int vertex) const { return coords_[vertex]; }

  // 顶点 v 的出边，以连续区间 [begin, end) 访问。
  const GraphEdge* EdgesBegin(int vertex) const {
    return edges_.data() + offsets_[vertex];
  }
  const GraphEdge* EdgesEnd(int vertex) const {
    return edges_.data() + offsets_[vertex + 1];
  }

  // 按全局边索引访问任意边（Yen 中从 ShortestPath::edges 还原边属性）。
  const GraphEdge& EdgeAt(int global_idx) const { return edges_[global_idx]; }
  int EdgeCount() const { return static_cast<int>(edges_.size()); }

  // 批量只读访问——供 bfdb 序列化使用
  const std::vector<Coordinate>& coords() const { return coords_; }
  const std::vector<int>& offsets() const { return offsets_; }
  const std::vector<GraphEdge>& edges() const { return edges_; }

 private:
  friend class GraphBuilder;

  std::vector<Coordinate> coords_;  // 逐顶点坐标，大小 V
  std::vector<int> offsets_;        // CSR 行偏移，大小 V + 1
  std::vector<GraphEdge> edges_;    // CSR 边数组，大小 E
};

}  // namespace px

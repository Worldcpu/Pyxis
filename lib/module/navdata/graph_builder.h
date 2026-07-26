#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "px/core/ident.h"
#include "px/core/nav_graph.h"
#include "px/module/navdata/nav_data_ir.h"

namespace px {

// 从原始导航数据构建不可变 NavGraph (CSR)。
// 构造函数一次性完成解析和构建。
class GraphBuilder {
 public:
  GraphBuilder(const std::vector<RawWaypoint>& waypoints,
               const std::vector<RawSegment>& segments,
               const std::vector<RawAirport>& airports = {});

  // 从 bfdb 缓存反序列化重建图——直接接管已解码的向量。
  GraphBuilder(std::vector<Coordinate> coords, std::vector<int> offsets,
               std::vector<GraphEdge> edges, std::vector<Ident> idents,
               std::vector<std::string> airway_names, std::vector<WaypointKind> kinds,
               std::vector<bool> has_outbound, std::vector<bool> has_inbound);

  const NavGraph& graph() const;
  const Ident& IdentOf(int vertex) const;
  WaypointKind KindOf(int vertex) const;
  const std::string& AirwayName(int airway_id) const;
  int AirwayCount() const;

  // 批量访问——供序列化 (bfdb) 使用。
  const std::vector<Ident>& Idents() const { return idents_; }
  const std::vector<WaypointKind>& Kinds() const { return kinds_; }
  const std::vector<std::string>& AirwayNames() const { return airway_names_; }
  const std::vector<bool>& HasOutboundVec() const { return has_outbound_; }
  const std::vector<bool>& HasInboundVec() const { return has_inbound_; }

  bool HasOutbound(int vertex) const;
  bool HasInbound(int vertex) const;

  int VertexByIdent(const Ident& key) const;
  int VertexByAirport(const std::string& icao) const;

 private:
  NavGraph graph_;
  std::vector<Ident> idents_;
  std::vector<WaypointKind> kinds_;
  std::vector<bool> has_outbound_;
  std::vector<bool> has_inbound_;
  std::vector<std::string> airway_names_;

  std::unordered_map<Ident, int, std::hash<Ident>> ident_index_;
  std::unordered_map<std::string, int> airport_index_;
};

}  // namespace px

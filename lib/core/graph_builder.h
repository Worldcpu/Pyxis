#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "px/core/ident.h"
#include "px/core/nav_data_ir.h"
#include "px/core/nav_graph.h"

namespace px {

// 从原始导航数据构建不可变 NavGraph (CSR)。
// 构造函数一次性完成解析和构建。
class GraphBuilder {
 public:
  GraphBuilder(const std::vector<RawWaypoint>& waypoints,
               const std::vector<RawSegment>& segments);

  const NavGraph& graph() const;
  const Ident& IdentOf(int vertex) const;
  const std::string& AirwayName(int airway_id) const;
  int AirwayCount() const;

  bool HasOutbound(int vertex) const;
  bool HasInbound(int vertex) const;

  int VertexByIdent(const Ident& key) const;
  int VertexByAirport(const std::string& icao) const;

 private:
  NavGraph graph_;
  std::vector<Ident> idents_;
  std::vector<bool> has_outbound_;
  std::vector<bool> has_inbound_;
  std::vector<std::string> airway_names_;

  std::unordered_map<Ident, int, std::hash<Ident>> ident_index_;
  std::unordered_map<std::string, int> airport_index_;
};

}  // namespace px

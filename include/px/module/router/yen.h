#pragma once

#include <vector>

#include "px/core/astar.h"
#include "px/core/cancel_token.h"
#include "px/core/result.h"

namespace px {

// Yen K-最短路径配置。
struct YenOptions {
  SearchOptions search;  // 约束、启发式传递给每次 A*
  const CancelToken* cancel = nullptr;
};

// 返回从 start 到 goal 最多 k 条无环最短路径，按有效代价升序排列。
// k 超出 [1, 8] 范围时返回 Error{kInvalidInput}。
Result<std::vector<ShortestPath>> FindKShortestPaths(const NavGraph& graph,
                                                     int start, int goal, int k,
                                                     const YenOptions& options);

}  // namespace px

#pragma once

#include <cstdint>

namespace px {

// 航段所属高度层结构（低空 Victor / 高空 Jet / 两者通用）。
// 底层类型 uint8_t，使 GraphEdge 保持 16 字节紧凑布局。
// 枚举值被序列化写入缓存格式，禁止重排序。
enum class AirwayLevel : uint8_t {
  kLow,   // 低空航路
  kHigh,  // 高空航路
  kBoth,  // 高/低空通用
};
static_assert(static_cast<uint8_t>(AirwayLevel::kLow) == 0 &&
                  static_cast<uint8_t>(AirwayLevel::kHigh) == 1 &&
                  static_cast<uint8_t>(AirwayLevel::kBoth) == 2,
              "AirwayLevel 枚举值是缓存格式的一部分，禁止重排序");

// 航路方向限制，编码自原始数据的方向字段。
enum class AirwayDirection {
  kBoth,     // 'N' — 无限制，双向可用
  kForward,  // 'F' — 仅 from→to
  kBackward  // 'B' — 仅 to→from
};

// CSR 图中的有向边。16 字节 POD——边数组是图中最大的结构，
// A* 热路径遍历它，紧凑布局让每条 cache line 容纳两倍的边数。
// distance_nm 存为 float（单段精度 ~2 米，远低于噪声），
// 路径代价由搜索以 double 累加，跨航路不会丢失精度。
struct GraphEdge {
  int32_t to = -1;                // 目标顶点索引
  float distance_nm = 0.0f;       // 大圆距离（海里）
  uint16_t airway_id = 0;         // 航路名表索引（0 = "DCT" 合成边）
  int16_t base_fl = 0;            // 最低可用飞行高度层（0 = 无限制）
  int16_t top_fl = 0;             // 最高可用飞行高度层（0 = 无限制）
  AirwayLevel level = AirwayLevel::kLow;
};
static_assert(sizeof(GraphEdge) == 16, "GraphEdge 预期为 16 字节");

}  // namespace px

#pragma once

// bfdb 图段编码/解码器。将航路图结构序列化为紧凑的二进制格式，
// 所有引用指向共享的全局字符串池。
//
// 图段布局（与 bravofinder 的 bfdb graph section 一致）：
//   header   : U32 vertex_count, U32 edge_count, U32 airway_count
//   顶点记录 [V] : I32 lat_1e7, I32 lon_1e7,
//                  U32 ident_off, U32 ident_len,
//                  U32 region_off, U32 region_len,
//                  U8 flags (bit0=has_outbound, bit1=has_inbound),
//                  U8 kind (WaypointKind)
//   CSR offsets [V+1] : I32
//   边记录 [E] : I32 to, F32 distance_nm, U16 airway_id,
//                I16 base_fl, I16 top_fl, U8 level (AirwayLevel)
//   航路名 [airway_count] : U32 name_off, U32 name_len
//
// 参考：bravofinder/lib/io/cache/graph_codec.{h,cc}

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "px/core/coordinate.h"
#include "px/core/graph_edge.h"
#include "px/core/ident.h"
#include "px/core/result.h"
#include "px/module/navdata/nav_data_ir.h"
#include "byte_io.h"

namespace px {

// -------------------------------------------------------------------
// 坐标序列化：I32 乘以 1e7 的固定精度编码。
// -------------------------------------------------------------------
inline int32_t CoordToI7(double degrees) {
  return static_cast<int32_t>(std::round(degrees * 10000000.0));
}
inline double I7ToCoord(int32_t vertex_count) {
  return static_cast<double>(vertex_count) / 10000000.0;
}

// -------------------------------------------------------------------
// 解码结果：所有向量与编码器的参数一一对应。
// -------------------------------------------------------------------
struct DecodedGraph {
  std::vector<Coordinate> coords;          // 顶点坐标
  std::vector<int> offsets;                // CSR 行偏移，大小 V+1
  std::vector<GraphEdge> edges;            // CSR 边数组
  std::vector<Ident> idents;               // (ident, region) 字符串
  std::vector<std::string> airway_names;   // 航路名称表
  std::vector<WaypointKind> vert_kinds;    // 顶点类型
  std::vector<uint8_t> has_outbound;       // 是否有出边（0/1）
  std::vector<uint8_t> has_inbound;        // 是否有入边（0/1）
};

// -------------------------------------------------------------------
// 将航路图编码为图段字节。所有字符串通过 pool 去重。
// 返回的字节可直接写入 bfdb 文件 graph section。
// -------------------------------------------------------------------
inline Result<std::string> EncodeGraph(const std::vector<Coordinate>& coords,
                                       const std::vector<int>& offsets,
                                       const std::vector<GraphEdge>& edges,
                                       const std::vector<Ident>& idents,
                                       const std::vector<std::string>& airway_names,
                                       const std::vector<WaypointKind>& vert_kinds,
                                       const std::vector<uint8_t>& has_outbound,
                                       const std::vector<uint8_t>& has_inbound,
                                       StringPool& pool) {
  const size_t vertex_count = coords.size();
  const size_t edge_count = edges.size();

  // 基本一致性检查
  if (idents.size() != vertex_count || vert_kinds.size() != vertex_count ||
      has_outbound.size() != vertex_count || has_inbound.size() != vertex_count) {
    return tl::make_unexpected(Error(ErrorCode::kInvalidInput,
                                     "encode graph: per-vertex array sizes inconsistent"));
  }
  if (offsets.size() != vertex_count + 1) {
    return tl::make_unexpected(Error(ErrorCode::kInvalidInput,
                                     "encode graph: offsets size != vertex_count + 1"));
  }
  if (airway_names.size() > 0xFFFF) {
    return tl::make_unexpected(Error(ErrorCode::kInvalidInput,
                                     "encode graph: too many airway names (> 65535)"));
  }

  std::string out;
  ByteWriter w(out);

  // 预估容量并预留
  // header(4+4+4) + vertex_records(vertex_count*26) + offsets((vertex_count+1)*4) +
  // edges(edge_count*15) + airway_refs(airway_count*8)
  w.Reserve(12 + vertex_count * 26 + (vertex_count + 1) * 4 + edge_count * 15 +
            airway_names.size() * 8);

  // Header 计数
  w.WriteU32(static_cast<uint32_t>(vertex_count));
  w.WriteU32(static_cast<uint32_t>(edge_count));
  w.WriteU32(static_cast<uint32_t>(airway_names.size()));

  // 顶点记录
  for (size_t i = 0; i < vertex_count; ++i) {
    w.WriteI32(CoordToI7(coords[i].latitude));
    w.WriteI32(CoordToI7(coords[i].longitude));

    const auto ir = pool.Add(idents[i].ident);
    w.WriteU32(ir.first);
    w.WriteU32(ir.second);
    const auto rr = pool.Add(idents[i].region);
    w.WriteU32(rr.first);
    w.WriteU32(rr.second);

    uint8_t flags = 0;
    if (has_outbound[i]) flags |= 0x01;
    if (has_inbound[i]) flags |= 0x02;
    w.WriteU8(flags);
    w.WriteU8(static_cast<uint8_t>(vert_kinds[i]));
  }

  // CSR offsets
  for (int off : offsets) {
    w.WriteI32(off);
  }

  // 边记录
  for (const GraphEdge& ed : edges) {
    w.WriteI32(ed.to);
    w.WriteFloat(ed.distance_nm);
    w.WriteU16(ed.airway_id);
    w.WriteI16(ed.base_fl);
    w.WriteI16(ed.top_fl);
    w.WriteU8(static_cast<uint8_t>(ed.level));
  }

  // 航路名
  for (const std::string& name : airway_names) {
    const auto nr = pool.Add(name);
    w.WriteU32(nr.first);
    w.WriteU32(nr.second);
  }

  return Ok(std::move(out));
}

// -------------------------------------------------------------------
// 从图段字节和全局池 blob 解码出 DecodedGraph。
// -------------------------------------------------------------------
inline Result<DecodedGraph> DecodeGraph(const std::string& section_bytes,
                                        const std::string& pool_blob) {
  ByteReader r(section_bytes.data(), section_bytes.size());

  auto bad = [](const char* why) {
    return tl::make_unexpected(
        Error(ErrorCode::kCacheCorrupt,
              std::string("corrupt bfdb graph section: ") + why));
  };

  DecodedGraph graph;
  const uint32_t vertex_count = r.ReadU32();
  const uint32_t edge_count = r.ReadU32();
  const uint32_t airway_count = r.ReadU32();

  // 熔断检查：每个计数必须在剩余字节的合理范围内
  if (!r.Ok()) {
    return bad("truncated header");
  }

  // 顶点记录 26 B, CSR offsets 4 B, 边记录 15 B, 航路引用 8 B
  const size_t avail = r.Remaining();
  auto count_fits = [&](uint32_t count, size_t per_elem) {
    return static_cast<size_t>(count) <= avail / per_elem;
  };
  if (!count_fits(vertex_count, 26) || !count_fits(edge_count, 15) || !count_fits(airway_count, 8)) {
    return bad("counts exceed section size");
  }
  const size_t min_body = static_cast<size_t>(vertex_count) * 26 +
                          static_cast<size_t>(vertex_count + 1) * 4 +
                          static_cast<size_t>(edge_count) * 15 +
                          static_cast<size_t>(airway_count) * 8;
  if (min_body > avail) {
    return bad("combined counts exceed section size");
  }

  // 定点引用池引用，解码后统一解析字符串
  struct IdentRef {
    uint32_t ident_off, ident_len, region_off, region_len;
  };
  std::vector<IdentRef> ident_refs(vertex_count);

  // 顶点记录
  graph.coords.resize(vertex_count);
  graph.has_outbound.assign(vertex_count, 0);
  graph.has_inbound.assign(vertex_count, 0);
  graph.vert_kinds.resize(vertex_count);

  for (uint32_t i = 0; i < vertex_count; ++i) {
    graph.coords[i].latitude = I7ToCoord(r.ReadI32());
    graph.coords[i].longitude = I7ToCoord(r.ReadI32());
    ident_refs[i].ident_off = r.ReadU32();
    ident_refs[i].ident_len = r.ReadU32();
    ident_refs[i].region_off = r.ReadU32();
    ident_refs[i].region_len = r.ReadU32();
    const uint8_t flags = r.ReadU8();
    graph.has_outbound[i] = (flags & 0x01) ? 1 : 0;
    graph.has_inbound[i] = (flags & 0x02) ? 1 : 0;
    graph.vert_kinds[i] = static_cast<WaypointKind>(r.ReadU8());
  }

  // CSR offsets
  graph.offsets.resize(static_cast<size_t>(vertex_count) + 1);
  for (size_t i = 0; i <= vertex_count; ++i) {
    graph.offsets[i] = r.ReadI32();
  }

  // CSR 偏移量验证
  if (graph.offsets.front() != 0 ||
      graph.offsets.back() != static_cast<int>(edge_count)) {
    return bad("CSR offsets do not span [0, edge count]");
  }
  for (uint32_t i = 0; i < vertex_count; ++i) {
    if (graph.offsets[i] > graph.offsets[i + 1]) {
      return bad("CSR offsets are not monotonic");
    }
  }

  // 边记录
  graph.edges.resize(edge_count);
  for (uint32_t i = 0; i < edge_count; ++i) {
    GraphEdge ed;
    ed.to = r.ReadI32();
    ed.distance_nm = r.ReadFloat();
    ed.airway_id = r.ReadU16();
    ed.base_fl = r.ReadI16();
    ed.top_fl = r.ReadI16();
    ed.level = static_cast<AirwayLevel>(r.ReadU8());
    // 校验边引用
    if (ed.to < 0 || static_cast<uint32_t>(ed.to) >= vertex_count) {
      return bad("edge target vertex out of range");
    }
    if (ed.airway_id >= airway_count) {
      return bad("edge airway-name index out of range");
    }
    if (static_cast<uint8_t>(ed.level) >
        static_cast<uint8_t>(AirwayLevel::kBoth)) {
      return bad("edge airway level out of range");
    }
    graph.edges[i] = ed;
  }

  // 航路名引用（暂存偏移量，后续解析）
  struct NameRef {
    uint32_t off, len;
  };
  std::vector<NameRef> airway_refs(airway_count);
  for (uint32_t i = 0; i < airway_count; ++i) {
    airway_refs[i].off = r.ReadU32();
    airway_refs[i].len = r.ReadU32();
  }

  // 校验顶点 kind
  for (WaypointKind k : graph.vert_kinds) {
    if (static_cast<uint8_t>(k) > static_cast<uint8_t>(WaypointKind::kOther)) {
      return bad("vertex kind out of range");
    }
  }

  // 检查是否完全消耗且无错误
  if (!r.Ok()) {
    return bad("truncated or corrupt");
  }
  if (r.Remaining() != 0) {
    return bad("trailing bytes");
  }

  // 解析池引用
  bool refs_ok = true;
  graph.idents.resize(vertex_count);
  for (uint32_t i = 0; i < vertex_count; ++i) {
    const IdentRef& ir = ident_refs[i];
    std::string id = ResolveRef(pool_blob, ir.ident_off, ir.ident_len, refs_ok);
    std::string reg =
        ResolveRef(pool_blob, ir.region_off, ir.region_len, refs_ok);
    graph.idents[i] = Ident(std::move(id), std::move(reg));
  }
  graph.airway_names.resize(airway_count);
  for (uint32_t i = 0; i < airway_count; ++i) {
    graph.airway_names[i] =
        ResolveRef(pool_blob, airway_refs[i].off, airway_refs[i].len, refs_ok);
  }
  if (!refs_ok) {
    return bad("pool reference out of range");
  }

  return Ok(std::move(graph));
}

}  // namespace px

#pragma once

// 小端字节序序列化工具，Pyxis bfdb 缓存模块的公共基础设施。
//
// 所有整型按最低有效字节优先写入，与主机字节序无关。
// 浮点类型通过 IEEE-754 位模式（memcpy 到同宽度无符号整数）存储，
// 确保跨架构精确往返。
//
// 参考：bravofinder/lib/io/cache/byte_io.h

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>

namespace px {

// ----------------------------------------------------------------
// 小端字节序检查（C++17 兼容）
// ----------------------------------------------------------------
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline constexpr bool kIsBigEndian = true;
#else
inline constexpr bool kIsBigEndian = false;
#endif

// ----------------------------------------------------------------
// ByteWriter：向内存缓冲区追加小端编码数据。
// ----------------------------------------------------------------
class ByteWriter {
 public:
  explicit ByteWriter(std::string& out) : out_(out) {}

  // 预留额外容量，减少连续写入时的重分配次数。
  void Reserve(size_t extra) { out_.reserve(out_.size() + extra); }

  void WriteU8(uint8_t v) { out_.push_back(static_cast<char>(v)); }

  void WriteU16(uint16_t v) {
    WriteU8(static_cast<uint8_t>(v));
    WriteU8(static_cast<uint8_t>(v >> 8));
  }

  void WriteU32(uint32_t v) {
    for (int i = 0; i < 4; ++i) {
      WriteU8(static_cast<uint8_t>(v >> (8 * i)));
    }
  }

  void WriteU64(uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      WriteU8(static_cast<uint8_t>(v >> (8 * i)));
    }
  }

  void WriteI16(int16_t v) { WriteU16(static_cast<uint16_t>(v)); }
  void WriteI32(int32_t v) { WriteU32(static_cast<uint32_t>(v)); }

  void WriteFloat(float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    WriteU32(bits);
  }

  void WriteDouble(double v) {
    uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    WriteU64(bits);
  }

  // 追加原始字节（不做字节序转换）。用于预序列化的二进制块。
  void WriteBytes(const char* data, size_t n) { out_.append(data, n); }

  // 长度前缀字符串：U32 长度 + 原始字节。ByteReader::ReadString 的逆操作。
  void WriteString(const std::string& s) {
    WriteU32(static_cast<uint32_t>(s.size()));
    out_.append(s);
  }

  // 当前缓冲区内容指针
  const char* Data() const { return out_.data(); }
  // 当前已写入字节数
  size_t Size() const { return out_.size(); }

 private:
  std::string& out_;
};

// ----------------------------------------------------------------
// ByteReader：从字节区间读取小端编码数据，带越界检查。
// ----------------------------------------------------------------
class ByteReader {
 public:
  ByteReader(const char* data, size_t size) : data_(data), size_(size) {}

  // 所有读取操作是否成功（未越界）。
  bool Ok() const { return ok_; }
  size_t Remaining() const { return ok_ ? size_ - pos_ : 0; }

  uint8_t ReadU8() {
    if (pos_ + 1 > size_) {
      ok_ = false;
      return 0;
    }
    return static_cast<uint8_t>(data_[pos_++]);
  }

  uint16_t ReadU16() {
    uint16_t lo = ReadU8();
    uint16_t hi = ReadU8();
    return static_cast<uint16_t>(lo | (hi << 8));
  }

  uint32_t ReadU32() {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      v |= static_cast<uint32_t>(ReadU8()) << (8 * i);
    }
    return v;
  }

  uint64_t ReadU64() {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      v |= static_cast<uint64_t>(ReadU8()) << (8 * i);
    }
    return v;
  }

  int16_t ReadI16() { return static_cast<int16_t>(ReadU16()); }
  int32_t ReadI32() { return static_cast<int32_t>(ReadU32()); }

  float ReadFloat() {
    uint32_t bits = ReadU32();
    float v = 0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
  }

  double ReadDouble() {
    uint64_t bits = ReadU64();
    double v = 0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
  }

  // 读取 n 字节原始数据到 dst。越界时设置错误标志且不复制。
  void ReadBytes(char* dst, size_t n) {
    if (pos_ + n > size_) {
      ok_ = false;
      return;
    }
    std::memcpy(dst, data_ + pos_, n);
    pos_ += n;
  }

  // 读取长度前缀字符串。越界时设置错误标志并返回空串。
  std::string ReadString() {
    const uint32_t len = ReadU32();
    if (!ok_ || len > Remaining()) {
      ok_ = false;
      return {};
    }
    std::string s(len, '\0');
    std::memcpy(s.data(), data_ + pos_, len);
    pos_ += len;
    return s;
  }

  // 批量解码 n 个小端 int16 值到 dst。小端主机上为单次 memcpy。
  // 用于大数组（如 64800 单元格的 MORA 网格）。
  void ReadI16Span(int16_t* dst, size_t n) {
    if (pos_ + n * sizeof(int16_t) > size_) {
      ok_ = false;
      return;
    }
    std::memcpy(dst, data_ + pos_, n * sizeof(int16_t));
    pos_ += n * sizeof(int16_t);
    if constexpr (kIsBigEndian) {
      for (size_t i = 0; i < n; ++i) {
        dst[i] = static_cast<int16_t>(ByteSwap16(static_cast<uint16_t>(dst[i])));
      }
    }
  }

  // 批量解码 n 个小端 int32 值到 dst。用于 CSR offset 数组等。
  void ReadI32Span(int32_t* dst, size_t n) {
    if (pos_ + n * sizeof(int32_t) > size_) {
      ok_ = false;
      return;
    }
    std::memcpy(dst, data_ + pos_, n * sizeof(int32_t));
    pos_ += n * sizeof(int32_t);
    if constexpr (kIsBigEndian) {
      for (size_t i = 0; i < n; ++i) {
        dst[i] = static_cast<int32_t>(ByteSwap32(static_cast<uint32_t>(dst[i])));
      }
    }
  }

 private:
  static uint16_t ByteSwap16(uint16_t v) {
    return static_cast<uint16_t>((v << 8) | (v >> 8));
  }
  static uint32_t ByteSwap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) |
           ((v & 0xFF000000u) >> 24);
  }

  const char* data_;
  size_t size_;
  size_t pos_ = 0;
  bool ok_ = true;
};

// ----------------------------------------------------------------
// StringPool：字符串池，每条不同字符串仅存储一份，返回 (offset, length) 引用。
// ----------------------------------------------------------------
class StringPool {
 public:
  StringPool() = default;

  // 向池中添加字符串。若已存在则返回已有的引用。
  // 返回 (offset_in_blob, length) 对。
  std::pair<uint32_t, uint32_t> Add(const std::string& s) {
    auto [it, inserted] = interned_.try_emplace(s, std::pair<uint32_t, uint32_t>{});
    if (!inserted) {
      return it->second;
    }
    const std::pair<uint32_t, uint32_t> ref{
        static_cast<uint32_t>(blob_.size()),
        static_cast<uint32_t>(s.size())};
    blob_.append(s);
    it->second = ref;
    return ref;
  }

  // 池的原始二进制内容。
  const std::string& blob() const { return blob_; }

  // 将池序列化到 ByteWriter。
  void WriteTo(ByteWriter& w) const {
    w.WriteBytes(blob_.data(), blob_.size());
  }

 private:
  std::string blob_;
  std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> interned_;
};

// ----------------------------------------------------------------
// 池引用解析：从池 blob 中按 (offset, len) 提取字符串。
// ----------------------------------------------------------------

// 从 std::string 池中解析引用。解析失败时设置 ok = false。
inline std::string ResolveRef(const std::string& blob, uint32_t offset,
                              uint32_t len, bool& ok) {
  if (offset > blob.size() || len > blob.size() - offset) {
    ok = false;
    return {};
  }
  return blob.substr(offset, len);
}

// 从原始字节区间解析引用（避免复制池数据）。
inline std::string ResolveRef(const char* pool, size_t pool_len,
                              uint32_t offset, uint32_t len, bool& ok) {
  if (offset > pool_len || len > pool_len - offset) {
    ok = false;
    return {};
  }
  return std::string(pool + offset, len);
}

}  // namespace px

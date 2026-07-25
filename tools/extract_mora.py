#!/usr/bin/env python3
"""从 Fenix .db3 或 PMDG .s3db 导航数据库中提取 MORA 网格数据。

输出: 原始二进制文件, 64800 个 int16_t (小端序), 行主序 (lat+90)*360+(lon+180)。
表格式与 bravofinder LoadGridMora (lib/io/loaders/dfd1/dfd1_loader.cc:361) 兼容。

用法:
  python3 tools/extract_mora.py --db navdata/fenix_navdata.db3 --output navdata/mora_grid.bin
  python3 tools/extract_mora.py --db navdata/PMDG_navdata.s3db --output navdata/mora_grid.bin
"""

import argparse
import sqlite3
import struct
import sys
from pathlib import Path

# 网格维度 (bravofinder MoraGrid 常量)
LAT_COUNT = 180  # -90 .. +89
LON_COUNT = 360  # -180 .. +179
CELL_COUNT = LAT_COUNT * LON_COUNT  # 64800
MORA_COLUMNS = [f"mora{i:02d}" for i in range(1, 31)]  # mora01..mora30


def find_mora_table(cursor: sqlite3.Cursor) -> str | None:
    """在数据库中定位 MORA 表 (Fenix: GridMora, PMDG: tbl_grid_mora)。"""
    cursor.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
        " AND (name='GridMora' OR name='tbl_grid_mora');"
    )
    rows = cursor.fetchall()
    if rows:
        return rows[0][0]
    return None


def parse_mora_value(raw: str) -> int | None:
    """将 TEXT(3) MORA 值解析为整数 FL。

    "010" -> 10, "UNK" / 空串 / 非数值 -> None。
    """
    if raw is None:
        return None
    text = raw.strip()
    if not text or text.upper() == "UNK":
        return None
    try:
        return int(text)
    except ValueError:
        return None


def extract_mora(db_path: str) -> list[int]:
    """从数据库提取 MORA 网格, 返回 64800 个 int16 值列表。"""
    if not Path(db_path).exists():
        raise FileNotFoundError(f"数据库文件不存在: {db_path}")

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    table = find_mora_table(cursor)
    if table is None:
        conn.close()
        raise ValueError("数据库中未找到 GridMora 或 tbl_grid_mora 表")

    cells = [0] * CELL_COUNT

    columns = ["starting_latitude", "starting_longitude"] + MORA_COLUMNS
    col_str = ", ".join(columns)
    cursor.execute(f"SELECT {col_str} FROM {table};")

    written = 0  # 写入次数 (含重复覆盖)
    for row in cursor.fetchall():
        lat = row[0]
        lon0 = row[1]

        for i in range(30):
            value = parse_mora_value(row[2 + i])
            if value is None or value <= 0:
                continue

            lon = lon0 + i
            # 边界检查 (bravofinder Index() 逻辑)
            if lat < -90 or lat > 89 or lon < -180 or lon > 179:
                continue

            idx = (lat + 90) * LON_COUNT + (lon + 180)
            cells[idx] = value
            written += 1

    conn.close()

    # 统计 (去重计数)
    populated = sum(1 for v in cells if v != 0)
    lat_min = 90
    lat_max = -90
    for lat in range(-90, 90):
        for lon in range(-180, 180):
            if cells[(lat + 90) * LON_COUNT + (lon + 180)] != 0:
                lat_min = min(lat_min, lat)
                lat_max = max(lat_max, lat)

    total = CELL_COUNT
    ratio = 100.0 * populated / total if total > 0 else 0.0
    print(f"[*] 表: {table}")
    print(f"[*] 总单元格: {total} | 写入次数: {written}")
    print(f"[*] 去重后填充: {populated} ({ratio:.1f}%)")
    print(f"[*] 纬度覆盖: {lat_min} .. {lat_max}")

    return cells


def write_binary(cells: list[int], output_path: str) -> None:
    """将单元格写入小端序 int16_t 二进制文件。"""
    packed = struct.pack(f"<{len(cells)}h", *cells)
    Path(output_path).write_bytes(packed)
    size_kb = len(packed) / 1024
    print(f"[*] 已写入: {output_path} ({size_kb:.1f} KB)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="从导航数据库提取 MORA 网格为二进制文件"
    )
    parser.add_argument(
        "--db", required=True,
        help="Fenix .db3 或 PMDG .s3db 数据库路径"
    )
    parser.add_argument(
        "--output", required=True,
        help="输出二进制文件路径 (64800 x int16_t 小端)"
    )
    args = parser.parse_args()

    try:
        cells = extract_mora(args.db)
        write_binary(cells, args.output)
    except (FileNotFoundError, ValueError, sqlite3.Error) as e:
        print(f"[!] 错误: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

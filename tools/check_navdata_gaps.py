#!/usr/bin/env python3
"""
查询 Fenix 和 PMDG 导航数据中的限制/异常。

查询 1: 只有进近程序(IAP)但没有 STAR 程序的机场
查询 2: 缺少区域标识的航路（例如中国 W 航路应有 Z 开头的 ICAO 区域码）

用法:
  python3 tools/check_navdata_gaps.py                           # 默认 navdata 路径
  python3 tools/check_navdata_gaps.py --pmdg /path/to/pmdg.db   # 指定 PMDG 数据库
  python3 tools/check_navdata_gaps.py --fenix /path/to/fenix.db # 指定 Fenix 数据库
  python3 tools/check_navdata_gaps.py --detail                  # 列出具体机场/航路
  python3 tools/check_navdata_gaps.py --db pmdg                 # 仅查询 PMDG
  python3 tools/check_navdata_gaps.py --db fenix                # 仅查询 Fenix
"""

import argparse
import sqlite3
import sys
from pathlib import Path
from typing import List, Tuple, Set, Dict


# ── 默认路径 ──────────────────────────────────────────────
DEFAULT_NAVDATA_DIR = Path(__file__).resolve().parent.parent / "navdata"
DEFAULT_PMDG = DEFAULT_NAVDATA_DIR / "PMDG_navdata.s3db"
DEFAULT_FENIX = DEFAULT_NAVDATA_DIR / "fenix_navdata.db3"


# ═══════════════════════════════════════════════════════════
# 查询 1: 只有进近程序没有 STAR 的机场
# ═══════════════════════════════════════════════════════════

def query1_pmdg(db_path: Path) -> Tuple[int, List[str], int, int]:
    """
    PMDG: 对比 tbl_iaps 和 tbl_stars 中出现的 airport_identifier。
    返回 (仅有IAP的机场数, 机场列表, IAP机场总数, STAR机场总数)
    """
    conn = sqlite3.connect(str(db_path))

    iap_airports: Set[str] = set()
    star_airports: Set[str] = set()

    for row in conn.execute("SELECT DISTINCT airport_identifier FROM tbl_iaps"):
        iap_airports.add(row[0])

    for row in conn.execute("SELECT DISTINCT airport_identifier FROM tbl_stars"):
        star_airports.add(row[0])

    conn.close()

    only_iap = sorted(iap_airports - star_airports)
    return len(only_iap), only_iap, len(iap_airports), len(star_airports)


def query1_fenix(db_path: Path) -> Tuple[int, List[Tuple[str, str]], int, int]:
    """
    Fenix: Terminals 表中 Proc=3 为进近, Proc=1 为 STAR。
    通过 AirportID 关联 Airports 表获取 ICAO 代码。
    返回 (仅有IAP的机场数, [(ICAO, 示例进近名), ...], IAP机场总数, STAR机场总数)
    """
    conn = sqlite3.connect(str(db_path))

    # 有进近程序的机场 (Proc=3)
    iap_rows = conn.execute("""
        SELECT DISTINCT a.ICAO, t.FullName
        FROM Terminals t
        JOIN Airports a ON t.AirportID = a.ID
        WHERE t.Proc = '3'
    """).fetchall()

    # 有 STAR 的机场 (Proc=1)
    star_rows = conn.execute("""
        SELECT DISTINCT a.ICAO
        FROM Terminals t
        JOIN Airports a ON t.AirportID = a.ID
        WHERE t.Proc = '1'
    """).fetchall()

    conn.close()

    # 构建映射: ICAO → 示例进近名
    iap_map: Dict[str, str] = {}
    for r in iap_rows:
        if r[0] not in iap_map:
            iap_map[r[0]] = r[1]

    iap_airports = set(iap_map.keys())
    star_airports = {r[0] for r in star_rows}

    only_iap_icaos = sorted(iap_airports - star_airports)
    result = [(icao, iap_map[icao]) for icao in only_iap_icaos]

    return len(result), result, len(iap_airports), len(star_airports)


# ═══════════════════════════════════════════════════════════
# 查询 2: 缺少区域标识的航路
# ═══════════════════════════════════════════════════════════

def query2_pmdg(db_path: Path) -> Tuple[int, List[str], int]:
    """
    PMDG: 检查 tbl_enroute_airways 中各航路是否存在 icao_code 或
    area_code 为空/无效的情况。
    航路由 (area_code, route_identifier) 唯一确定。
    返回 (有问题的航路数, 详情列表, 总航路数)
    """
    conn = sqlite3.connect(str(db_path))

    # 总航路数（按 area_code + route_identifier 去重）
    total = conn.execute("""
        SELECT COUNT(DISTINCT area_code || '|' || route_identifier)
        FROM tbl_enroute_airways
    """).fetchone()[0]

    # 检查1: 航路中存在任何 waypoint 的 icao_code 为空的航路
    null_icao_rows = conn.execute("""
        SELECT DISTINCT area_code, route_identifier, COUNT(*) AS wp_count
        FROM tbl_enroute_airways
        WHERE icao_code IS NULL OR icao_code = ''
        GROUP BY area_code, route_identifier
    """).fetchall()

    # 检查2: 航路中存在任何 waypoint 的 area_code 为空的航路
    null_area_rows = conn.execute("""
        SELECT DISTINCT area_code, route_identifier, COUNT(*) AS wp_count
        FROM tbl_enroute_airways
        WHERE area_code IS NULL OR area_code = ''
        GROUP BY area_code, route_identifier
    """).fetchall()

    # 检查3: 航路中存在 icao_code 格式异常的 waypoint
    # ICAO 区域码应为 2 字符字母数字组合，如 ZB, K2, EG
    weird_icao_rows = conn.execute("""
        SELECT DISTINCT area_code, route_identifier, icao_code
        FROM tbl_enroute_airways
        WHERE icao_code NOT GLOB '[A-Z][A-Z0-9]'
        ORDER BY area_code, route_identifier
        LIMIT 30
    """).fetchall()

    conn.close()

    problems: List[str] = []

    if null_icao_rows:
        for r in null_icao_rows:
            problems.append(
                f"航路 {r[0]}/{r[1]}: "
                f"{r[2]} 个航路点的 icao_code 为空"
            )

    if null_area_rows:
        for r in null_area_rows:
            problems.append(
                f"航路 {r[0]}/{r[1]}: "
                f"{r[2]} 个航路点的 area_code 为空"
            )

    if weird_icao_rows:
        problems.append(
            f"发现 {len(weird_icao_rows)}+ 个 icao_code 格式异常 "
            f"(非2字符字母数字组合), 示例: "
            + ", ".join(
                f"{r[0]}/{r[1]}[{r[2]}]"
                for r in weird_icao_rows[:5]
            )
        )

    return len(null_icao_rows) + len(null_area_rows), problems, total


def query2_fenix(db_path: Path) -> Tuple[int, List[str], int]:
    """
    Fenix: Airways 表无直接区域字段，通过 AirwayLegs → Waypoints →
    WaypointLookup 链获取 Country。

    检查每条航路是否所有航路点都有有效的 Country (ICAO 区域码)。
    返回 (有问题的航路数, 详情列表, 总航路数)
    """
    conn = sqlite3.connect(str(db_path))

    # 总航路数
    total = conn.execute("SELECT COUNT(*) FROM Airways").fetchone()[0]

    # 查找缺少 Country 的航路点 → 追溯到航路
    null_country_rows = conn.execute("""
        SELECT DISTINCT aw.Ident AS airway_ident, w.Ident AS wpt_ident,
               wl.Country
        FROM Airways aw
        JOIN AirwayLegs al ON aw.ID = al.AirwayID
        JOIN Waypoints w ON al.Waypoint1ID = w.ID
        LEFT JOIN WaypointLookup wl ON w.Ident = wl.Ident
        WHERE wl.Country IS NULL OR wl.Country = ''
           OR wl.ID IS NULL
        GROUP BY aw.Ident
        LIMIT 50
    """).fetchall()

    conn.close()

    problems = []
    if null_country_rows:
        for r in null_country_rows:
            problems.append(
                f"航路 {r[0]}: "
                f"航路点 {r[1]} 缺少 Country (WaypointLookup 无匹配或 Country 为空)"
            )

    return len(null_country_rows), problems, total


# ═══════════════════════════════════════════════════════════
# 输出
# ═══════════════════════════════════════════════════════════

def print_header(title: str) -> None:
    print()
    print("=" * 60)
    print(f"  {title}")
    print("=" * 60)


def print_query1(db_name: str, count: int, detail_list: list, total_iap: int,
                 total_star: int, show_detail: bool) -> None:
    """格式化输出查询1结果"""
    print_header(f"[{db_name}] 查询1: 仅有进近没有STAR的机场")
    print(f"  有进近程序的机场总数:  {total_iap}")
    print(f"  有 STAR  程序的机场总数: {total_star}")
    print(f"  仅有进近、无 STAR 的机场数: {count}")
    if total_iap > 0 and count > 0:
        print(f"  占比: {count / total_iap * 100:.1f}% (相对于有进近的机场)")

    if show_detail and count > 0 and detail_list:
        print(f"\n  详细列表 (前 50):")
        for item in detail_list[:50]:
            if isinstance(item, tuple):
                icao, sample = item
                print(f"    {icao}  示例进近: {sample}")
            else:
                print(f"    {item}")
        if len(detail_list) > 50:
            print(f"    ... 还有 {len(detail_list) - 50} 个机场")


def print_query2(db_name: str, problem_count: int, problems: List[str],
                 total: int, show_detail: bool) -> None:
    """格式化输出查询2结果"""
    print_header(f"[{db_name}] 查询2: 缺少区域标识的航路")
    print(f"  航路总数: {total}")
    print(f"  有区域标识问题的航路数: {problem_count}")

    if problem_count > 0:
        print(f"  占比: {problem_count / total * 100:.1f}%")
        if show_detail and problems:
            print(f"\n  详细列表 (前 30):")
            for p in problems[:30]:
                print(f"    {p}")
            if len(problems) > 30:
                print(f"    ... 还有 {len(problems) - 30} 条")


# ═══════════════════════════════════════════════════════════
# 入口
# ═══════════════════════════════════════════════════════════

def main() -> None:
    parser = argparse.ArgumentParser(
        description="查询 Fenix 和 PMDG 导航数据中的限制/异常",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                        # 使用默认路径查询两个数据库
  %(prog)s --db pmdg --detail     # 仅查询 PMDG，显示详细信息
  %(prog)s --pmdg custom_pmdg.s3db --fenix custom_fenix.db3
        """,
    )
    parser.add_argument(
        "--pmdg", type=Path, default=DEFAULT_PMDG,
        help=f"PMDG 数据库路径 (默认: {DEFAULT_PMDG})"
    )
    parser.add_argument(
        "--fenix", type=Path, default=DEFAULT_FENIX,
        help=f"Fenix 数据库路径 (默认: {DEFAULT_FENIX})"
    )
    parser.add_argument(
        "--db", choices=["pmdg", "fenix", "both"], default="both",
        help="指定查询哪个数据库 (默认: both)"
    )
    parser.add_argument(
        "--detail", action="store_true",
        help="列出具体的机场名和航路名"
    )
    args = parser.parse_args()

    # ── 执行查询 ──────────────────────────────────────

    if args.db in ("pmdg", "both"):
        if not args.pmdg.exists():
            print(f"[错误] PMDG 数据库不存在: {args.pmdg}", file=sys.stderr)
        else:
            c1, l1, tiap, tstar = query1_pmdg(args.pmdg)
            print_query1("PMDG", c1, l1, tiap, tstar, args.detail)

            c2, p2, t2 = query2_pmdg(args.pmdg)
            print_query2("PMDG", c2, p2, t2, args.detail)

    if args.db in ("fenix", "both"):
        if not args.fenix.exists():
            print(f"[错误] Fenix 数据库不存在: {args.fenix}", file=sys.stderr)
        else:
            c1, l1, tiap, tstar = query1_fenix(args.fenix)
            print_query1("Fenix", c1, l1, tiap, tstar, args.detail)

            c2, p2, t2 = query2_fenix(args.fenix)
            print_query2("Fenix", c2, p2, t2, args.detail)

    print()


if __name__ == "__main__":
    main()

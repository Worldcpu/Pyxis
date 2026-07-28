#!/usr/bin/env python3
"""Fenix navdata 格式验证工具.

对照 fenix_loader.cc 的所有格式假设，逐项检查实际数据库。
三阶段: A) 已知约束+未知值发现  B) 结构性探索  C) 静默失败汇总.
"""

import os, sys, sqlite3
from collections import Counter, defaultdict

# --- helpers ---

VALID_PATH_TERMINATORS = {
    "TF", "IF", "DF", "CF", "AF", "RF",
    "CA", "FA", "VA", "HA", "CD", "FD", "VD",
    "CI", "VI", "CR", "VR", "FC", "FM", "VM",
    "PI", "HM", "HF",
}
VALID_AIRWAY_LEVELS = {"L", "H", "B", "1", "2"}
VALID_TURN_DIR = {"L", "R"}
VALID_PROC = {"1", "2"}
VALID_NAVAID_TYPES = set(range(1, 9))
NAVAID_NAMES = {1:"VOR",2:"DME",3:"NDB",4:"TACAN",5:"VOR-DME",6:"VORTAC",7:"ILS/DME",8:"NDB-DME"}
EXPECTED_INT_COLS = {  # 代码用 ColumnInt() / ColumnOptInt() 读取的列
    "config.val", "WaypointLookup.ID", "Navaids.ID", "Navaids.Type",
    "Waypoints.ID", "Waypoints.NavaidID", "Airways.ID",
    "AirwayLegs.AirwayID", "AirwayLegs.Waypoint1ID", "AirwayLegs.Waypoint2ID",
    "AirwayLegs.IsStart", "AirwayLegs.IsEnd",
    "Airports.ID", "Airports.Elevation",
    "Runways.AirportID", "Runways.Elevation",
    "Holdings.minimum_altitude", "Holdings.maximum_altitude", "Holdings.holding_speed",
    "GridMora.starting_latitude", "GridMora.starting_longitude",
    "Terminals.ID", "Terminals.AirportID",
    "TerminalLegs.ID", "TerminalLegs.TerminalID", "TerminalLegs.WptID",
    "TerminalLegsEx.ID",
}
EXPECTED_DOUBLE_COLS = {  # ColumnDouble() / ColumnOptDouble()
    "Navaids.Freq", "Navaids.Range",
    "Waypoints.Latitude", "Waypoints.Longtitude",
    "Airports.Latitude", "Airports.Longtitude",
    "Runways.Latitude", "Runways.Longtitude",
    "Holdings.inbound_holding_course", "Holdings.leg_time", "Holdings.leg_length",
    "TerminalLegs.Course", "TerminalLegs.Distance",
    "TerminalLegsEx.SpeedLimit",
}

class Reporter:
    def __init__(self):
        self.lines = []
        self.counts = {"ERROR": 0, "WARN": 0, "INFO": 0, "PASS": 0}
        self.silent = []  # 静默失败项

    def _add(self, level, tag, msg):
        self.lines.append((level, tag, msg))
        self.counts[level] = self.counts.get(level, 0) + 1

    def section(self, label):
        self.lines.append(("", "", f"\n  {label}"))

    def pass_(self, tag, msg=""):
        self._add("PASS", tag, msg)
    def warn(self, tag, msg):
        self._add("WARN", tag, msg)
    def error(self, tag, msg):
        self._add("ERROR", tag, msg)
    def info(self, tag, msg):
        self._add("INFO", tag, msg)
    def silent_warn(self, desc, count):
        self.silent.append(("WARN", desc, count))
    def silent_error(self, desc, count):
        self.silent.append(("ERROR", desc, count))

    def report(self):
        for level, tag, msg in self.lines:
            if tag:
                print(f"  [{level:5s}] {tag:30s} {msg}" if msg else f"  [{level:5s}] {tag}")
            else:
                print(msg)


def parse_fenix_alt(alt_text):
    """等价于 ParseFenixAlt 的 Python 实现，返回 (kind_str, val1, val2, error)."""
    if not alt_text:
        return ("kNone", 0, 0, None)
    pos = 0
    s = alt_text
    while pos < len(s) and s[pos].isdigit():
        pos += 1
    if pos == 0:
        return ("?", 0, 0, "无数字前缀")
    try:
        val1 = int(s[:pos])
    except ValueError:
        return ("?", 0, 0, f"第一段不可解析: '{s[:pos]}'")
    desc1 = s[pos] if pos < len(s) else '\0'
    pos += 1

    if desc1 == 'B' and pos < len(s):
        n2 = pos
        while pos < len(s) and s[pos].isdigit():
            pos += 1
        if pos > n2:
            try:
                val2 = int(s[n2:pos])
            except ValueError:
                return ("?", 0, 0, f"第二段不可解析: '{s[n2:pos]}'")
            desc2 = s[pos] if pos < len(s) else '\0'
            if desc2 in ('A', '+'):
                return ("kBetween", val1, val2, None)
            # desc2 非 A/+  → fall through to single

    kind = "kAt"
    if desc1 in ('+', 'A'):
        kind = "kAtOrAbove"
    elif desc1 in ('-', 'B'):
        kind = "kAtOrBelow"
    return (kind, val1, 0, None)


def find_db():
    d = os.environ.get("BRAVOFINDER_NAVDATA", "navdata")
    for name in ["fenix_navdata.db3", "navdata.db3", "fenix.db3"]:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    if os.path.isdir(d):
        for f in os.listdir(d):
            if f.endswith(".db3"):
                return os.path.join(d, f)
    return None


def main():
    db_path = find_db()
    if not db_path:
        print("ERROR: Fenix .db3 not found. Set BRAVOFINDER_NAVDATA or place under navdata/")
        return 1

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    R = Reporter()

    # === helper: table/column introspection ===
    tables_have = {}
    all_tables = [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()]
    for tbl in all_tables:
        cur.execute(f"PRAGMA table_info('{tbl}')")
        tables_have[tbl] = {r[1]: r[2] for r in cur.fetchall()}  # col_name → type

    # ================================================================
    # 阶段 A: 已知约束 + 未知值发现
    # ================================================================
    print("===== 阶段 A: 已知约束 + 未知值发现 =====")

    # --- §0 表/列存在性 ---
    R.section("§0  表/列存在性")
    expected = {
        "config":          ["key","val"],
        "WaypointLookup":  ["ID","Country"],
        "Navaids":         ["ID","Type","Ident","Elevation","Freq","Range"],
        "Waypoints":       ["ID","Ident","Latitude","Longtitude","NavaidID"],
        "Airways":         ["ID","Ident"],
        "AirwayLegs":      ["AirwayID","Level","Waypoint1ID","Waypoint2ID","IsStart","IsEnd"],
        "Airports":        ["ID","ICAO","Latitude","Longtitude","Elevation"],
        "Runways":         ["AirportID","Ident","Latitude","Longtitude","Elevation"],
        "Holdings":        ["waypoint_identifier","region_code","icao_code",
                            "inbound_holding_course","leg_time","leg_length",
                            "turn_direction","minimum_altitude","maximum_altitude",
                            "holding_speed"],
        "GridMora":        ["starting_latitude","starting_longitude"] +
                           [f"mora{i:02d}" for i in range(1,31)],
        "Terminals":       ["ID","AirportID","Proc","Name","Rwy"],
        "TerminalLegs":    ["ID","TerminalID","Transition","TrackCode",
                            "Course","Distance","Alt","TurnDir","WptID"],
        "TerminalLegsEx":  ["ID","SpeedLimit"],
    }
    missing_tbl = 0; missing_col = 0
    for tbl, cols in expected.items():
        if tbl not in tables_have:
            R.error("", f"表 '{tbl}' 不存在")
            missing_tbl += 1; continue
        for c in cols:
            if c not in tables_have[tbl]:
                R.error("", f"  {tbl}.{c} 缺失")
                missing_col += 1
        for c in list(tables_have[tbl].keys()):
            if c not in cols and c not in ("ID",):
                R.info("", f"  {tbl} 有额外列: '{c}'")
    if missing_tbl == 0 and missing_col == 0:
        R.pass_("", f"{len(expected)}/{len(expected)} 表, 列完整")
    else:
        R.error("", f"缺失 {missing_tbl} 表, {missing_col} 列 → 跳过后续")
        R.report(); return 1

    # --- §1 ParseFenixCycle ---
    R.section("§1  ParseFenixCycle")
    cur.execute("SELECT key, val FROM config")
    config = {k: v for k, v in cur.fetchall()}
    if "Cycle" not in config:
        R.warn("C1", "缺少 key='Cycle'")
    else:
        v = config["Cycle"]
        if v and v.strip().isdigit():
            R.pass_("C2", f"Cycle={v}")
        else:
            R.warn("C2", f"Cycle 不可解析: '{v}'")
    extra_keys = set(config.keys()) - {"Cycle"}
    if extra_keys:
        R.info("unknown-keys", f"额外 config key: {sorted(extra_keys)}")

    # --- §2 NavaidKindFromType ---
    R.section("§2  NavaidKindFromType")
    cur.execute("SELECT Type, COUNT(*) FROM Navaids GROUP BY Type ORDER BY Type")
    navaid_types = [(t, cnt) for t, cnt in cur.fetchall()]
    found_types = set()
    total_tacan = 0
    for t_str, cnt in navaid_types:
        try:
            t = int(t_str)
        except ValueError:
            R.warn("N9", f"Navaids.Type='{t_str}' 非整数, {cnt}行 → kOther")
            R.silent_warn(f"Navaids.Type='{t_str}' → kOther", cnt)
            continue
        found_types.add(t)
        name = NAVAID_NAMES.get(t, "?")
        if t == 4:
            total_tacan = cnt
            R.error("N4", f"TACAN(Type=4) → kVor, 枚举为 kDme=DME/TACAN ({cnt} 行)")
            R.silent_error(f"TACAN→kVor 枚举矛盾", cnt)
        elif t not in VALID_NAVAID_TYPES:
            R.warn("N9", f"Type={t}({name}) 未定义, {cnt}行 → kOther")
            R.silent_warn(f"Navaids.Type={t} → kOther", cnt)
        elif t == 6:
            R.info("N6", f"VORTAC(Type=6) → kVor (VOR优先, TACAN部件忽略), {cnt}行")
        elif t == 8:
            R.info("N8", f"NDB-DME(Type=8) → kNdb (简化, DME部件丢失), {cnt}行")
        else:
            R.pass_("", f"Type={t}({name}) → 正确, {cnt}行")
    unknown_labeled = sum(cnt for t_str, cnt in navaid_types
                          if (t_str.isdigit() and int(t_str) not in VALID_NAVAID_TYPES)
                          or not t_str.isdigit())
    if unknown_labeled == 0:
        R.info("", "所有 Type 均在 1-8 范围内")

    # --- §3 ParseFenixAlt ---
    R.section("§3  ParseFenixAlt")
    cur.execute("SELECT Alt, COUNT(*) FROM TerminalLegs WHERE Alt != '' AND Alt IS NOT NULL GROUP BY Alt")
    alt_vals = cur.fetchall()
    alt_by_kind = Counter()
    alt_bad = []
    for alt_text, cnt in alt_vals:
        kind_str, v1, v2, err = parse_fenix_alt(alt_text)
        if err:
            alt_bad.append((alt_text, cnt, err))
        else:
            alt_by_kind[kind_str] += cnt
    if alt_bad:
        for text, cnt, reason in alt_bad:
            # MAP = Missed Approach Point, 故意无高度约束, 合法
            if text == 'MAP':
                R.info("A?", f"'MAP' ×{cnt} (Missed Approach Point, kNone 合法)")
            else:
                R.warn("A?", f"无法解析: '{text}' ×{cnt}: {reason}")
                R.silent_warn(f"Alt '{text}' 无法解析 → kNone", cnt)
    if not [t for t,_,_ in alt_bad if t != 'MAP']:
        R.pass_("", f"{len(alt_vals)} 种唯一值全部可解析 (MAP 除外)")
    # 类别分布
    parts = []
    for k in ["kNone", "kAt", "kAtOrAbove", "kAtOrBelow", "kBetween"]:
        if alt_by_kind.get(k):
            parts.append(f"{k}={alt_by_kind[k]}")
    R.info("categories", ", ".join(parts))

    # 检查是否存在代码未预期的后缀字母
    all_suffixes = set()
    for alt_text, _ in alt_vals:
        s = alt_text
        pos = 0
        while pos < len(s) and s[pos].isdigit(): pos += 1
        if pos < len(s): all_suffixes.add(s[pos])
        if pos + 1 < len(s):
            pos2 = pos + 1
            while pos2 < len(s) and s[pos2].isdigit(): pos2 += 1
            if pos2 < len(s): all_suffixes.add(s[pos2])
    unexpected_suffixes = all_suffixes - {'A','B','+','-'}
    # 过滤掉来自 MAP 等非数字开头的值
    real_unexpected = set()
    for s in unexpected_suffixes:
        for alt_text, _ in alt_vals:
            if s in alt_text and any(c.isdigit() for c in alt_text[:alt_text.index(s)] if alt_text.index(s) > 0):
                break
    if unexpected_suffixes:
        R.info("alt-suffix", f"Alt 含非标后缀字母: {sorted(unexpected_suffixes)} (如 'M' 来自 'MAP')")

    # --- §4 LoadWaypoints ---
    R.section("§4  LoadWaypoints")
    # W5: Ident 长度
    cur.execute("SELECT COUNT(*) FROM Waypoints WHERE LENGTH(Ident) > 7")
    long_ident = cur.fetchone()[0]
    if long_ident:
        cur.execute("SELECT COUNT(*) FROM Waypoints")
        total_wp = cur.fetchone()[0]
        R.warn("W5", f"Ident > FixedIdent::kIdentCap(7): {long_ident}/{total_wp} → 将被 skip")
        R.silent_warn(f"Waypoints.Ident>7 → skip", long_ident)
        cur.execute("SELECT Ident, LENGTH(Ident) FROM Waypoints WHERE LENGTH(Ident) > 7 ORDER BY LENGTH(Ident) DESC LIMIT 10")
        for ident, l in cur.fetchall():
            R.info("", f"  '{ident}' len={l}")
    else:
        R.pass_("W5", "全部 Ident ≤ 7")
    # Ident 字符集
    cur.execute("SELECT COUNT(*) FROM Waypoints WHERE Ident != UPPER(Ident)")
    lower = cur.fetchone()[0]
    if lower: R.warn("", f"Ident 含小写: {lower} 行 → 匹配可能失败")
    cur.execute("SELECT COUNT(*) FROM Waypoints WHERE Ident GLOB '*[^A-Za-z0-9]*'")
    special = cur.fetchone()[0]
    if special: R.warn("", f"Ident 含特殊字符: {special} 行")

    # 坐标
    cur.execute("SELECT COUNT(*) FROM Waypoints WHERE Latitude < -90 OR Latitude > 90")
    if cur.fetchone()[0]: R.warn("W-", f"纬度越界 [-90,90]")
    cur.execute("SELECT COUNT(*) FROM Waypoints WHERE Longtitude < -180 OR Longtitude > 180")
    if cur.fetchone()[0]: R.warn("W-", f"经度越界 [-180,180]")

    # NavaidID 引用
    cur.execute("SELECT COUNT(*) FROM Waypoints w WHERE w.NavaidID > 0 AND w.NavaidID NOT IN (SELECT ID FROM Navaids)")
    orphan_nav = cur.fetchone()[0]
    if orphan_nav: R.warn("W6", f"NavaidID 引用缺失: {orphan_nav} → kFix")

    # WaypointLookup.Country
    cur.execute("SELECT COUNT(*) FROM WaypointLookup WHERE Country != UPPER(Country)")
    lower_c = cur.fetchone()[0]
    if lower_c: R.warn("W7", f"Country 含小写: {lower_c}")
    cur.execute("SELECT LENGTH(Country), COUNT(*) FROM WaypointLookup GROUP BY 1 ORDER BY 1")
    for l, cnt in cur.fetchall():
        if l > 2: R.info("", f"Country 长度={l}: {cnt} 行")

    # --- §5 LoadNavaidDetails (列存在性已覆盖) ---

    # --- §6 LoadAirways ---
    R.section("§6  LoadAirways")
    # Level
    cur.execute("SELECT Level, COUNT(*) FROM AirwayLegs GROUP BY Level ORDER BY COUNT(*) DESC")
    levels = cur.fetchall()
    unknown_levels = [(l, c) for l, c in levels if l not in VALID_AIRWAY_LEVELS]
    level_summary = ", ".join(f"'{l}'={c}" for l, c in levels)
    if unknown_levels:
        for l, c in unknown_levels:
            R.warn("R4", f"Level='{l}' ×{c} → kLow (静默 fallback)")
            R.silent_warn(f"AirwayLegs.Level='{l}' → kLow", c)
    else:
        R.pass_("R4", level_summary)
    # FK orphans
    cur.execute("SELECT COUNT(*) FROM AirwayLegs al LEFT JOIN Airways a ON al.AirwayID=a.ID WHERE a.ID IS NULL")
    oa = cur.fetchone()[0]
    if oa: R.warn("R5", f"AirwayID FK 孤儿: {oa} → skip整行")
    cur.execute("SELECT COUNT(*) FROM AirwayLegs al LEFT JOIN Waypoints w ON al.Waypoint1ID=w.ID WHERE w.ID IS NULL")
    ow1 = cur.fetchone()[0]
    if ow1: R.warn("R6", f"Waypoint1ID FK 孤儿: {ow1} → skip整行")
    cur.execute("SELECT COUNT(*) FROM AirwayLegs al LEFT JOIN Waypoints w ON al.Waypoint2ID=w.ID WHERE w.ID IS NULL")
    ow2 = cur.fetchone()[0]
    if ow2: R.warn("R7", f"Waypoint2ID FK 孤儿: {ow2} → skip整行")
    if oa == 0 and ow1 == 0 and ow2 == 0:
        R.pass_("R5-R7", "外键完整")
    # IsStart/IsEnd
    cur.execute("SELECT IsStart, COUNT(*) FROM AirwayLegs GROUP BY IsStart")
    isstart_dist = cur.fetchall()
    if len(isstart_dist) > 1:
        R.info("R8", f"IsStart/IsEnd SELECT了但未使用, 值分布: {isstart_dist}")

    # --- §7 LoadAirports ---
    R.section("§7  LoadAirports")
    # ICAO 格式
    cur.execute("SELECT COUNT(*) FROM Airports WHERE ICAO != UPPER(ICAO)")
    lower_icao = cur.fetchone()[0]
    if lower_icao: R.warn("P2", f"ICAO 含小写: {lower_icao}")
    cur.execute("SELECT COUNT(*) FROM Airports WHERE LENGTH(ICAO) != 4")
    bad_len = cur.fetchone()[0]
    if bad_len:
        cur.execute("SELECT ICAO FROM Airports WHERE LENGTH(ICAO) != 4 LIMIT 10")
        samples = [r[0] for r in cur.fetchall()]
        R.warn("P2", f"ICAO 长度≠4: {bad_len}, 样本: {samples}")
    # 有程序但无 region
    cur.execute("""
        SELECT COUNT(DISTINCT a.ICAO) FROM Airports a
        JOIN Terminals t ON t.AirportID=a.ID
        WHERE a.ICAO NOT IN (
          SELECT DISTINCT a2.ICAO FROM Airports a2
          JOIN Terminals t2 ON t2.AirportID=a2.ID
          JOIN TerminalLegs tl ON tl.TerminalID=t2.ID
          JOIN WaypointLookup wl ON wl.ID=tl.WptID
          WHERE wl.Country != ''
        )
    """)
    no_region = cur.fetchone()[0]
    if no_region: R.info("P3", f"{no_region} 机场有程序但无法从 WaypointLookup 获取 region")
    # 多 Country
    cur.execute("""
        SELECT a.ICAO, wl.Country, COUNT(*) FROM Airports a
        JOIN Terminals t ON t.AirportID=a.ID
        JOIN TerminalLegs tl ON tl.TerminalID=t.ID
        JOIN WaypointLookup wl ON wl.ID=tl.WptID
        WHERE wl.Country!='' GROUP BY a.ICAO, wl.Country
    """)
    by_icao = defaultdict(list)
    for icao, country, cnt in cur.fetchall():
        by_icao[icao].append((country, cnt))
    multi = [(i, r) for i, r in by_icao.items() if len(r) > 1]
    if multi:
        R.info("P4", f"{len(multi)} 机场有多个 Country 码 (try_emplace 取首个): " +
               ", ".join(f"{i}{r}" for i, r in sorted(multi, key=lambda x: -sum(c for _,c in x[1]))[:5]))

    # --- §8 LoadHoldings ---
    R.section("§8  LoadHoldings")
    # turn_direction
    cur.execute("SELECT turn_direction, COUNT(*) FROM Holdings GROUP BY turn_direction ORDER BY COUNT(*) DESC")
    td_dist = [(td, cnt) for td, cnt in cur.fetchall()]
    unknown_td = [(td, cnt) for td, cnt in td_dist if td not in ('L','R')]
    td_summary = ", ".join(f"'{td}'={cnt}" for td, cnt in td_dist)
    if unknown_td:
        for td, cnt in unknown_td:
            R.warn("H3", f"turn_direction='{td}' ×{cnt} → 'R' (静默)")
            R.silent_warn(f"Holdings.turn_direction 非L/R → 'R'", cnt)
    else:
        R.pass_("H3", td_summary)
    # 边界值
    cur.execute("SELECT COUNT(*) FROM Holdings WHERE minimum_altitude <= 0 AND minimum_altitude IS NOT NULL")
    if cur.fetchone()[0]: R.info("H4", f"min_alt ≤0 → 0")
    cur.execute("SELECT COUNT(*) FROM Holdings WHERE maximum_altitude >= 99999 OR maximum_altitude <= 0")
    if cur.fetchone()[0]: R.info("H5", f"max_alt ≥99999或≤0 → 0")
    cur.execute("SELECT COUNT(*) FROM Holdings WHERE holding_speed <= 0 AND holding_speed IS NOT NULL")
    if cur.fetchone()[0]: R.info("H6", f"holding_speed ≤0 → 0")
    # leg_time/leg_length NULL
    cur.execute("SELECT COUNT(*) FROM Holdings WHERE leg_time IS NULL OR leg_time < 0")
    null_time = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM Holdings WHERE leg_length IS NULL OR leg_length < 0")
    null_len = cur.fetchone()[0]
    if null_time or null_len:
        R.info("H7-H8", f"leg_time NULL={null_time}, leg_length NULL={null_len} (→ -1=无限制)")
    # icao_code
    cur.execute("SELECT COUNT(*) FROM Holdings WHERE icao_code = '' OR icao_code IS NULL")
    empty_icao = cur.fetchone()[0]
    if empty_icao: R.info("H2", f"icao_code 空 → 'ENRT': {empty_icao}")

    # --- §9 LoadMoraGrid ---
    R.section("§9  LoadMoraGrid")
    cur.execute("SELECT COUNT(*) FROM GridMora")
    total_mora_rows = cur.fetchone()[0]
    nonempty = 0; parse_fail = 0
    for i in range(1, 31):
        col = f"mora{i:02d}"
        cur.execute(f"SELECT COUNT(*) FROM GridMora WHERE {col} != '' AND {col} IS NOT NULL")
        nonempty += cur.fetchone()[0]
    empty = total_mora_rows * 30 - nonempty
    R.info("M2-M3", f"{total_mora_rows}行×30列, {nonempty} 非空, {empty} 空")
    # 坐标变换前范围
    cur.execute("SELECT COUNT(*) FROM GridMora")
    if total_mora_rows:
        cur.execute("SELECT MIN(starting_latitude), MAX(starting_latitude), MIN(starting_longitude), MAX(starting_longitude) FROM GridMora")
        mn_lat, mx_lat, mn_lon, mx_lon = cur.fetchone()
        R.info("M6-M8", f"lat [{mn_lat},{mx_lat}], lon [{mn_lon},{mx_lon}]")

    # --- §10 LoadAllRunways (列存在性已覆盖) ---

    # --- §11/12 TerminalLegs ---
    R.section("§11 TerminalLegs (批量+单机场共用约束)")

    # Proc
    cur.execute("SELECT Proc, COUNT(*) FROM Terminals GROUP BY Proc ORDER BY COUNT(*) DESC")
    procs = cur.fetchall()
    proc_summary = ", ".join(f"'{p}'={c}" for p, c in procs)
    unknown_proc = [(p, c) for p, c in procs if p not in VALID_PROC]
    if unknown_proc:
        for p, c in unknown_proc:
            R.warn("T2", f"Proc='{p}' ×{c} → kApproach (静默)")
            R.silent_warn(f"Terminals.Proc='{p}' → kApproach", c)
    R.info("proc-dist", proc_summary)

    # TrackCode
    cur.execute("SELECT TrackCode, COUNT(*) FROM TerminalLegs GROUP BY TrackCode ORDER BY COUNT(*) DESC")
    tc_list = cur.fetchall()
    unknown_tc = [(tc, c) for tc, c in tc_list if (tc or "").strip() not in VALID_PATH_TERMINATORS]
    tc_summary = ", ".join(f"'{tc}'={c}" for tc, c in tc_list)
    if unknown_tc:
        for tc, c in unknown_tc:
            R.error("T5", f"TrackCode='{tc}' ×{c} → kUnknown→skip leg")
            R.silent_error(f"TrackCode='{tc}' → skip leg", c)
    else:
        # 确认每种值都在合法集合中
        all_ok = all((tc or "").strip() in VALID_PATH_TERMINATORS for tc, _ in tc_list)
        if all_ok:
            R.pass_("T5", f"{len(tc_list)} 种 TrackCode, 全部可识别")
    R.info("trackcode-dist", tc_summary)

    # TurnDir
    cur.execute("SELECT TurnDir, COUNT(*) FROM TerminalLegs GROUP BY TurnDir ORDER BY COUNT(*) DESC")
    tds = cur.fetchall()
    unknown_td = [(td, c) for td, c in tds if td and td.strip() not in VALID_TURN_DIR]
    td_summary = ", ".join(f"'{td}'={c}" for td, c in tds)
    if unknown_td:
        for td, c in unknown_td:
            R.warn("T9", f"TurnDir='{td}' ×{c} → '\\0'")
            R.silent_warn(f"TurnDir='{td}' → '\\0'", c)
    else:
        R.pass_("T9", td_summary)

    # Alt 见 §3

    # FK orphans
    cur.execute("SELECT COUNT(*) FROM TerminalLegs tl LEFT JOIN Terminals t ON tl.TerminalID=t.ID WHERE t.ID IS NULL")
    o_tid = cur.fetchone()[0]
    if o_tid: R.warn("FK", f"TerminalID FK 孤儿: {o_tid}")

    cur.execute("SELECT COUNT(*) FROM TerminalLegs tl WHERE tl.WptID > 0 AND tl.WptID NOT IN (SELECT ID FROM WaypointLookup)")
    o_wpl = cur.fetchone()[0]
    if o_wpl: R.warn("FK", f"WptID 在 WaypointLookup 中缺失: {o_wpl}")

    cur.execute("SELECT COUNT(*) FROM TerminalLegsEx ex LEFT JOIN TerminalLegs tl ON ex.ID=tl.ID WHERE tl.ID IS NULL")
    o_ex = cur.fetchone()[0]
    if o_ex: R.warn("FK", f"TerminalLegsEx.ID FK 孤儿: {o_ex}")

    # Transition 模式
    cur.execute("SELECT CASE WHEN Transition='ALL' THEN 'ALL' WHEN Transition LIKE 'RW%' AND LENGTH(Transition)<=6 THEN 'RWxx' WHEN Transition='' OR Transition IS NULL THEN '(empty)' ELSE 'OTHER' END AS cat, COUNT(*) FROM TerminalLegs GROUP BY cat ORDER BY COUNT(*) DESC")
    trans_cats = cur.fetchall()
    trans_other = [(c, n) for c, n in trans_cats if c == 'OTHER']
    if trans_other:
        cur.execute("SELECT COUNT(DISTINCT Transition) FROM TerminalLegs WHERE Transition NOT LIKE 'RW%' AND Transition != 'ALL' AND Transition != '' AND Transition IS NOT NULL")
        n_distinct = cur.fetchone()[0]
        R.info("T11", f"Transition 命名过渡点: {trans_other[0][1]} 行, {n_distinct} 种唯一值 (SID/STAR 标准)")

    # SpeedLimit NULL/负
    cur.execute("SELECT COUNT(*) FROM TerminalLegsEx WHERE SpeedLimit IS NULL OR SpeedLimit < 0")
    neg_spd = cur.fetchone()[0]
    if neg_spd: R.info("T10", f"SpeedLimit NULL/负: {neg_spd} → 0")

    # WptID NULL/≤0
    cur.execute("SELECT COUNT(*) FROM TerminalLegs WHERE WptID IS NULL OR WptID <= 0")
    null_wpt = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM TerminalLegs")
    total_tlegs = cur.fetchone()[0]
    if null_wpt: R.info("T6", f"WptID NULL/≤0: {null_wpt}/{total_tlegs} → fix 为空")

    # Waypoint ident > 7 in procedure context
    cur.execute("""SELECT COUNT(*) FROM TerminalLegs tl
        JOIN Waypoints w ON tl.WptID=w.ID
        WHERE LENGTH(w.Ident) > 7""")
    proc_long = cur.fetchone()[0]
    if proc_long: R.warn("T7", f"procedure leg waypoint Ident>7: {proc_long} → fix 为空")

    # RNP cross-loader gap
    has_rnp = any('rnp' in c.lower() for c in tables_have.get('TerminalLegs', {})) or \
              any('rnp' in c.lower() for c in tables_have.get('TerminalLegsEx', {}))
    if not has_rnp:
        R.warn("RNP", "Fenix schema 无 RNP 列 — dfd1/dfd2/xplane12 全部加载 rnp_centinm")
        R.silent_warn("缺失 RNP → rnp_centinm 永远为 0", 0)

    # Name 空值
    cur.execute("SELECT COUNT(*) FROM Terminals WHERE Name IS NULL OR Name = ''")
    empty_name = cur.fetchone()[0]
    if empty_name: R.warn("", f"Terminals.Name 空: {empty_name}")

    # Course 范围
    cur.execute("SELECT COUNT(*) FROM TerminalLegs WHERE Course IS NOT NULL AND (Course < 0 OR Course > 360)")
    bad_course = cur.fetchone()[0]
    if bad_course: R.warn("", f"Course 越界 [0,360]: {bad_course}")

    # Distance 负值
    cur.execute("SELECT COUNT(*) FROM TerminalLegs WHERE Distance IS NOT NULL AND Distance < 0")
    neg_dist = cur.fetchone()[0]
    if neg_dist: R.warn("", f"Distance 负值: {neg_dist}")

    # R.report() dumps accumulated lines
    # --- §14 坐标 ---
    R.section("§14 坐标范围")
    for tbl, lat, lon in [("Waypoints","Latitude","Longtitude"),
                           ("Airports","Latitude","Longtitude"),
                           ("Runways","Latitude","Longtitude")]:
        cur.execute(f"SELECT COUNT(*) FROM {tbl} WHERE {lat} < -90 OR {lat} > 90")
        bl = cur.fetchone()[0]
        cur.execute(f"SELECT COUNT(*) FROM {tbl} WHERE {lon} < -180 OR {lon} > 180")
        bl2 = cur.fetchone()[0]
        if bl or bl2:
            R.warn("", f"{tbl}: lat越界={bl}, lon越界={bl2}")

    # --- §15 外键汇总 ---
    R.section("§15 外键完整性")
    fks = [
        ("AirwayLegs.AirwayID→Airways.ID", "AirwayLegs al LEFT JOIN Airways a ON al.AirwayID=a.ID WHERE a.ID IS NULL"),
        ("AirwayLegs.Waypoint1ID→Waypoints.ID", "AirwayLegs al LEFT JOIN Waypoints w ON al.Waypoint1ID=w.ID WHERE w.ID IS NULL"),
        ("AirwayLegs.Waypoint2ID→Waypoints.ID", "AirwayLegs al LEFT JOIN Waypoints w ON al.Waypoint2ID=w.ID WHERE w.ID IS NULL"),
        ("Runways.AirportID→Airports.ID", "Runways r LEFT JOIN Airports a ON r.AirportID=a.ID WHERE a.ID IS NULL"),
        ("Terminals.AirportID→Airports.ID", "Terminals t LEFT JOIN Airports a ON t.AirportID=a.ID WHERE a.ID IS NULL"),
        ("TerminalLegs.TerminalID→Terminals.ID", "TerminalLegs tl LEFT JOIN Terminals t ON tl.TerminalID=t.ID WHERE t.ID IS NULL"),
        ("TerminalLegs.WptID→WaypointLookup.ID", "TerminalLegs tl WHERE tl.WptID>0 AND tl.WptID NOT IN (SELECT ID FROM WaypointLookup)"),
        ("TerminalLegsEx.ID→TerminalLegs.ID", "TerminalLegsEx ex LEFT JOIN TerminalLegs tl ON ex.ID=tl.ID WHERE tl.ID IS NULL"),
        ("Waypoints.NavaidID→Navaids.ID", "Waypoints w WHERE w.NavaidID>0 AND w.NavaidID NOT IN (SELECT ID FROM Navaids)"),
    ]
    any_orphan = False
    for label, sql in fks:
        cur.execute(f"SELECT COUNT(*) FROM {sql}")
        n = cur.fetchone()[0]
        if n:
            R.warn("", f"{label}: {n} orphan")
            any_orphan = True
    if not any_orphan:
        R.pass_("", "全部外键完整")

    # --- 打印阶段 A 累计输出 ---
    R.report()

    # ================================================================
    # 阶段 B: 结构性探索
    # ================================================================
    print("\n===== 阶段 B: 结构性探索 =====")

    # X1: 列类型 vs 代码预期
    R.section("X1  列类型对比")
    type_notes = 0
    for tbl, cols_info in tables_have.items():
        for col, coltype in cols_info.items():
            fq = f"{tbl}.{col}"
            if fq in EXPECTED_INT_COLS and "INT" not in coltype.upper():
                R.info("", f"{fq} 类型={coltype}, 代码用 ColumnInt → SQLite 隐式转换")
                type_notes += 1
            elif fq in EXPECTED_DOUBLE_COLS and not any(t in coltype.upper() for t in ("REAL","FLOAT","DOUBLE","NUM")):
                R.info("", f"{fq} 类型={coltype}, 代码用 ColumnDouble → SQLite 隐式转换")
                type_notes += 1
    if type_notes == 0:
        R.pass_("", "列类型与代码预期一致")

    # X2: 空表
    R.section("X2  空表检查")
    for tbl in expected:
        cur.execute(f"SELECT COUNT(*) FROM {tbl}")
        cnt = cur.fetchone()[0]
        if cnt == 0:
            R.warn("", f"表 '{tbl}' 为空 (0 行)")

    # X3: 高 NULL 率
    R.section("X3  高 NULL 率列")
    high_null = []
    for tbl in expected:
        for col in expected[tbl]:
            cur.execute(f"SELECT COUNT(*) FROM {tbl}")
            total = cur.fetchone()[0]
            if total == 0: continue
            cur.execute(f"SELECT COUNT(*) FROM {tbl} WHERE {col} IS NULL")
            null_cnt = cur.fetchone()[0]
            if null_cnt / total > 0.9:
                high_null.append(f"{tbl}.{col}={null_cnt}/{total}")
    if high_null:
        for h in high_null[:15]: R.info("", h)
    else:
        R.pass_("", "无 >90% NULL 列")

    # X4: 数值范围
    R.section("X4  数值范围合理性")
    for tbl, col, lo, hi, label in [
        ("Navaids","Freq",100,1200,"频率"),
        ("Navaids","Range",0.1,500,"距离"),
        ("TerminalLegs","Course",0,360,"航向"),
        ("TerminalLegs","Distance",0,9999,"距离"),
    ]:
        cur.execute(f"SELECT COUNT(*) FROM {tbl} WHERE {col} IS NOT NULL AND ({col} < {lo} OR {col} > {hi})")
        n = cur.fetchone()[0]
        if n:
            R.warn("", f"{tbl}.{col} 越界 [{lo},{hi}]: {n} ({label})")

    # X5: Ident 更深入的字符集
    R.section("X5  Ident 字符集深入")
    for tbl, col in [("Waypoints","Ident"),("Airways","Ident"),("Navaids","Ident")]:
        if tbl not in tables_have or col not in tables_have.get(tbl,{}): continue
        cur.execute(f"SELECT COUNT(*) FROM {tbl} WHERE {col} GLOB '*[^A-Z0-9 ]*'")
        n = cur.fetchone()[0]
        if n: R.warn("", f"{tbl}.{col} 含非大写字母数字: {n}")

    # X6: IsStart/IsEnd 数据
    R.section("X6  AirwayLegs.IsStart/IsEnd 数据")
    cur.execute("SELECT IsStart, IsEnd, COUNT(*) FROM AirwayLegs GROUP BY IsStart, IsEnd ORDER BY COUNT(*) DESC LIMIT 10")
    ie_dist = cur.fetchall()
    R.info("", f"IsStart/IsEnd 分布: {ie_dist}")

    # X7: TerminalLegs.ID 唯一性
    R.section("X7  TerminalLegs.ID 唯一性")
    cur.execute("SELECT ID, COUNT(*) FROM TerminalLegs GROUP BY ID HAVING COUNT(*) > 1 LIMIT 5")
    dups = cur.fetchall()
    if dups:
        R.error("", f"TerminalLegs.ID 重复: {dups}")
    else:
        R.pass_("", "ID 唯一")

    # X8: TerminalLegsEx 覆盖率
    R.section("X8  TerminalLegsEx 覆盖率")
    cur.execute("SELECT COUNT(*) FROM TerminalLegs")
    cur.execute("SELECT COUNT(*) FROM TerminalLegs tl JOIN TerminalLegsEx ex ON tl.ID=ex.ID")
    covered = cur.fetchone()[0]
    R.info("", f"TerminalLegs {covered}/{total_tlegs} 有 Ex 行")

    # X9: GridMora 步长
    R.section("X9  GridMora 步长")
    cur.execute("SELECT COUNT(DISTINCT starting_latitude), COUNT(DISTINCT starting_longitude) FROM GridMora")
    dist_lat, dist_lon = cur.fetchone()
    R.info("", f"唯一 lat={dist_lat}, lon={dist_lon} (总行={total_mora_rows})")

    # X10: Transition 模式 (已在 §11 覆盖)
    # X11: ICAO 格式 (已在 §7 覆盖)

    # ================================================================
    # 阶段 C: 静默失败汇总
    # ================================================================
    print("\n===== 阶段 C: 静默失败汇总 =====")
    if R.silent:
        for level, desc, count in R.silent:
            print(f"  [{level:5s}] {desc}: {count} 行受影响")
    else:
        print("  无静默失败项")

    # ================================================================
    # 汇总
    # ================================================================
    print(f"\n===== 汇总 =====")
    total_items = sum(R.counts.values())
    print(f"  ERROR: {R.counts['ERROR']:>3d}    WARN: {R.counts['WARN']:>3d}    INFO: {R.counts['INFO']:>3d}    PASS: {R.counts['PASS']:>3d}    TOTAL: {total_items}")
    if R.silent:
        s_errors = sum(1 for l,_,_ in R.silent if l == 'ERROR')
        s_warns = sum(1 for l,_,_ in R.silent if l == 'WARN')
        print(f"  静默失败: {s_errors} ERROR + {s_warns} WARN")

    conn.close()
    return 1 if R.counts["ERROR"] > 0 else 0


if __name__ == "__main__":
    sys.exit(main())

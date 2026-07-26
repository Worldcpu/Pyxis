#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""探查 PMDG .s3db / Fenix .db3 的完整 schema。

用法: python3 tools/parse_navdata.py [数据库路径]
默认: navdata/PMDG_navdata.s3db
"""

import os
import sys
import sqlite3

DEFAULT_DB = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                          "navdata", "PMDG_navdata.s3db")


def inspect_db_schema(db_path):
    if not os.path.exists(db_path):
        print(f"[!] 错误: 未找到数据库文件 {db_path}")
        return

    print(f"[*] 正在分析数据库结构: {db_path}")
    print("=" * 70)

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # 1. 查询数据库中所有的表名
    cursor.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';"
    )
    tables = [row[0] for row in cursor.fetchall()]

    if not tables:
        print("[!] 数据库中未找到任何有效的数据表！")
        return

    print(f"[+] 找到 {len(tables)} 个数据表: {', '.join(tables)}\n")

    # 2. 遍历每个表，输出所有的特征字段（列名和类型）及样本数据
    for table in tables:
        print(f"【 数据表名: {table} 】")

        # 使用 PRAGMA 查看表结构（字段名、数据类型、是否主键）
        cursor.execute(f"PRAGMA table_info('{table}');")
        columns = cursor.fetchall()

        col_names = []
        print("  包含的特征字段（列名及数据类型）:")
        for col in columns:
            cid, col_name, data_type, notnull, dflt_value, pk = col
            pk_tag = " [主键]" if pk else ""
            print(f"    - {col_name:<30} | 类型: {data_type:<10}{pk_tag}")
            col_names.append(col_name)

        # 查询总行数
        cursor.execute(f"SELECT COUNT(*) FROM '{table}';")
        total_rows = cursor.fetchone()[0]
        print(f"  总记录数: {total_rows} 条")

        # 打印 1 条示例数据帮助直观判断
        if total_rows > 0:
            cursor.execute(f"SELECT * FROM '{table}' LIMIT 1;")
            sample = cursor.fetchone()
            print("  样本数据字段 preview:")
            for name, val in zip(col_names, sample):
                print(f"    * {name:<28} = {val}")

        print("=" * 70)

    conn.close()


if __name__ == "__main__":
    db_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_DB
    inspect_db_schema(db_path)
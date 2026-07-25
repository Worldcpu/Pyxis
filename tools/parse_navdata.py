#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sqlite3

# 指定你的数据库路径
DB_PATH = "/home/ppm/Pyxis/lib/fenix_navdata.db3"


def inspect_db_schema():
    if not os.path.exists(DB_PATH):
        print(f"[!] 错误: 未找到数据库文件 {DB_PATH}")
        return

    print(f"[*] 正在分析数据库结构: {DB_PATH}")
    print("=" * 70)

    conn = sqlite3.connect(DB_PATH)
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
    inspect_db_schema()
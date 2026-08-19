from __future__ import annotations

import sqlite3
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: check_sqlite.py DATABASE")
        return 2

    database_path = Path(sys.argv[1]).resolve()
    connection = sqlite3.connect(f"{database_path.as_uri()}?mode=ro", uri=True)
    try:
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        print(f"integrity={integrity}")
        tables = [
            row[0]
            for row in connection.execute(
                "SELECT name FROM sqlite_master "
                "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
            )
        ]
        print(f"table_count={len(tables)}")
        for table in tables:
            escaped_table = table.replace('"', '""')
            count = connection.execute(f'SELECT count(1) FROM "{escaped_table}"').fetchone()[0]
            print(f"{table}={count}")
    finally:
        connection.close()
    return 0 if integrity == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())

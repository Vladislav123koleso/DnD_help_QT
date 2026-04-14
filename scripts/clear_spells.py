import sqlite3
import os

DB_PATHS = [
    "../dnd_rules.db",
    "../build/Desktop_Qt_6_8_1_MinGW_64_bit-Debug/debug/dnd_rules.db",
    "../build/Desktop_Qt_6_8_1_MinGW_64_bit-Debug/dnd_rules.db"
]

def clear_db(db_path):
    resolved_path = os.path.join(os.path.dirname(__file__), db_path)
    resolved_path = os.path.abspath(resolved_path)
    
    if not os.path.exists(resolved_path):
        print(f"Skipping {resolved_path} (not found)")
        return

    print(f"Clearing spells in: {resolved_path}")
    try:
        conn = sqlite3.connect(resolved_path)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM spells")
        # Reset autoincrement if desired, though not strictly necessary
        cursor.execute("DELETE FROM sqlite_sequence WHERE name='spells'") 
        conn.commit()
        
        count = cursor.execute("SELECT count(*) FROM spells").fetchone()[0]
        print(f"  -> Rows remaining: {count}")
        conn.close()
    except Exception as e:
        print(f"  -> Error: {e}")

if __name__ == "__main__":
    for path in DB_PATHS:
        clear_db(path)

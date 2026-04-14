import sqlite3
import json
import urllib.request
import os

import ssl

# URL to the 5e SRD Spells JSON
URL = "https://raw.githubusercontent.com/5e-bits/5e-database/main/src/2014/5e-SRD-Spells.json"
DB_FILE = "../dnd_rules.db"

def fetch_spells():
    print(f"Downloading spells from {URL}...")
    # Create an unverified context to avoid SSL errors on some Windows setups
    context = ssl._create_unverified_context()
    with urllib.request.urlopen(URL, context=context) as response:
        data = json.loads(response.read().decode())
    print(f"Downloaded {len(data)} spells.")
    return data

def init_db(cursor):
    # Ensure the table exists (matching the C++ schema)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS spells (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            level INTEGER,
            school TEXT,
            casting_time TEXT,
            range TEXT,
            components TEXT,
            duration TEXT,
            description TEXT
        )
    """)

def insert_spells(cursor, spells):
    print("Inserting spells into database...")
    count = 0
    for spell in spells:
        # Extract and format fields
        name = spell.get("name", "")
        level = spell.get("level", 0)
        
        school_data = spell.get("school", {})
        school = school_data.get("name", "") if isinstance(school_data, dict) else str(school_data)
        
        casting_time = spell.get("casting_time", "")
        range_val = spell.get("range", "")
        
        components_list = spell.get("components", [])
        components = ", ".join(components_list) if isinstance(components_list, list) else str(components_list)
        
        duration = spell.get("duration", "")
        
        desc_list = spell.get("desc", [])
        description = "\n".join(desc_list) if isinstance(desc_list, list) else str(desc_list)
        
        # Check if spell already exists to avoid duplicates
        cursor.execute("SELECT id FROM spells WHERE name = ?", (name,))
        if cursor.fetchone():
            continue

        cursor.execute("""
            INSERT INTO spells (name, level, school, casting_time, range, components, duration, description)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (name, level, school, casting_time, range_val, components, duration, description))
        count += 1
    
    print(f"Successfully inserted {count} new spells.")

def main():
    # Resolve absolute path for DB
    script_dir = os.path.dirname(os.path.abspath(__file__))
    db_path = os.path.join(script_dir, DB_FILE)
    
    print(f"Connecting to database at: {db_path}")
    
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    try:
        init_db(cursor)
        spells_data = fetch_spells()
        insert_spells(cursor, spells_data)
        conn.commit()
        print("Done!")
    except Exception as e:
        print(f"Error: {e}")
        conn.rollback()
    finally:
        conn.close()

if __name__ == "__main__":
    main()

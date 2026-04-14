import sqlite3
import json
import os
import re
import ast

DB_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'dnd_rules.db')
JSON_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'final_spells.json')

def clean_text(text):
    if not isinstance(text, str):
        return text
    
    if text == "None":
        return ""

    # 1. Handle Python dict strings (tables, lists) embedded in text
    # We look for patterns that look like dictionaries: {'type': ...}
    # Since they can be mixed with text, we might need to split or find them.
    # However, looking at the JSON, it seems they appear as standalone blocks or appended.
    # But wait, in the JSON example: "...пояснениями к ней.\n{'type': 'table'..."
    # So it's part of the string.
    
    def replace_dict_match(match):
        dict_str = match.group(0)
        try:
            data = ast.literal_eval(dict_str)
            if not isinstance(data, dict):
                return dict_str
            
            if data.get('type') == 'table':
                html = "<table border='1' cellspacing='0' cellpadding='5'>"
                if 'caption' in data:
                    html += f"<caption><b>{data['caption']}</b></caption>"
                
                if 'colLabels' in data:
                    html += "<tr>"
                    for col in data['colLabels']:
                        html += f"<th>{col}</th>"
                    html += "</tr>"
                
                if 'rows' in data:
                    for row in data['rows']:
                        html += "<tr>"
                        for cell in row:
                            html += f"<td>{cell}</td>"
                        html += "</tr>"
                html += "</table>"
                return html
            
            elif data.get('type') == 'list':
                tag = 'ul' if data.get('attrs', {}).get('type') == 'unordered' else 'ol'
                html = f"<{tag}>"
                for item in data.get('content', []):
                    # Recursively clean list items as they might contain tags too
                    html += f"<li>{clean_text(item)}</li>"
                html += f"</{tag}>"
                return html
                
        except:
            return dict_str # Return original if parsing fails
        return dict_str

    # Regex to find potential dicts. This is tricky because of nested braces.
    # Assuming they don't have nested dicts with braces for now, or are simple enough.
    # The example shows single line dicts.
    text = re.sub(r"\{'type':.*?\}(?=\s|$|\n)", replace_dict_match, text, flags=re.DOTALL)

    # 2. Remove custom tags like {@glossary text|url} -> text
    
    # Improved regex strategy to avoid cross-tag matching:
    # Match {@tag content|...} -> content (content cannot contain | or })
    text = re.sub(r"\{@[^\s}]+ ([^}|]+)\|[^}]+\}", r"\1", text)
    
    # Handle {@tag content} (like {@b Bold} or {@i Italic})
    # content cannot contain }
    text = re.sub(r"\{@b ([^}]+)\}", r"<b>\1</b>", text)
    text = re.sub(r"\{@i ([^}]+)\}", r"<i>\1</i>", text)
    
    # Catch-all for other tags {@tag content} -> content
    text = re.sub(r"\{@[^\s}]+ ([^}]+)\}", r"\1", text)
    
    # 3. Clean up English text in brackets like "Летучая мышь [Bat]" -> "Летучая мышь"
    # User specifically complained about [Bat], [Cat], etc.
    # Pattern: Space + [ + English chars + ]
    text = re.sub(r"\s\[[a-zA-Z0-9,\s\-\']+\]", "", text)

    return text

def create_db():
    print(f"Connecting to database: {DB_PATH}")
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    # Drop table if exists to ensure clean schema
    cursor.execute("DROP TABLE IF EXISTS spells")

    # Create table
    cursor.execute("""
        CREATE TABLE spells (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            level INTEGER,
            school TEXT,
            ritual BOOLEAN,
            casting_time TEXT,
            range TEXT,
            components TEXT,
            duration TEXT,
            description TEXT,
            classes TEXT,
            subclasses TEXT,
            source TEXT,
            upper TEXT
        )
    """)
    
    conn.commit()
    return conn

def fill_db(conn):
    print(f"Reading JSON from: {JSON_PATH}")
    try:
        with open(JSON_PATH, 'r', encoding='utf-8') as f:
            spells = json.load(f)
    except FileNotFoundError:
        print("Error: final_spells.json not found. Run the parser first.")
        return

    cursor = conn.cursor()
    count = 0
    
    for spell in spells:
        # Prepare data
        name = spell.get('name', 'Unknown')
        level = spell.get('level', 0)
        school = spell.get('school', '')
        
        # Ritual check: boolean field OR "Ритуал" in casting time
        ritual_bool = bool(spell.get('ritual', False))
        casting_time = spell.get('castingTime', '')
        if 'ритуал' in casting_time.lower():
            ritual_bool = True
        
        range_val = spell.get('range', '')
        components = spell.get('components', '')
        duration = spell.get('duration', '')
        description = clean_text(spell.get('description', ''))
        classes = ", ".join(spell.get('classes', [])) if isinstance(spell.get('classes'), list) else spell.get('classes', '')
        subclasses = spell.get('subclasses', '')
        source = spell.get('source', '')
        upper = clean_text(spell.get('upper', ''))

        cursor.execute("""
            INSERT INTO spells (name, level, school, ritual, casting_time, range, components, duration, description, classes, subclasses, source, upper)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (name, level, school, ritual_bool, casting_time, range_val, components, duration, description, classes, subclasses, source, upper))
        count += 1

    conn.commit()
    print(f"Successfully inserted {count} spells into the database.")

if __name__ == "__main__":
    conn = create_db()
    fill_db(conn)
    conn.close()

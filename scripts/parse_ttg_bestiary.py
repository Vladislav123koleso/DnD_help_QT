import requests
import json
import time
import re
import os

# ======================================================================================
# CONFIG & CONSTANTS
# ======================================================================================
MY_COOKIE = "_ga_WXD97L1HCX=GS2.1.s1751057246$o8$g0$t1751057246$j60$l0$h0; _ga=GA1.1.119112224.1730987303; _ym_uid=1730987305825672626; _ym_d=1746887959; ttg-theme=dark"

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept": "application/json, text/plain, */*",
    "Content-Type": "application/json",
    "Origin": "https://new.ttg.club",
    "Referer": "https://new.ttg.club/bestiary",
    "Cookie": MY_COOKIE
}

SEARCH_URL = "https://new.ttg.club/api/v2/bestiary/search"
DETAILS_URL_TEMPLATE = "https://new.ttg.club/api/v2/bestiary/{slug}"
OUTPUT_FILE = os.path.join(os.path.dirname(__file__), "final_bestiary.json")

# ======================================================================================
# TEXT CLEANING UTILS
# ======================================================================================
def clean_text(text):
    if not text or text == "None": return ""
    
    # Ensure text is string
    if not isinstance(text, str): 
        if isinstance(text, list):
            return "<br>".join([clean_text(t) for t in text])
        text = str(text)

    # 1. Complex tags with parameters {@tag text|params} -> text
    # Matches: {@glossary труднопроходимой местностью|url:difficult-terrain-phb} -> труднопроходимой местностью
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\s+([^}|]+)(?:\|[^}]*)?\}', r'\1', text)

    # 2. Simple tags with parameters {@tag text} -> text (if any remain)
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\s+([^}]+)\}', r'\1', text)

    # 3. Simple tags without parameters {@tag} -> "" (remove)
    # Be careful not to remove text if it formats like {@i text} -> text
    # The previous regexes handle the "space + text" case. 
    # This handles "no space" or "no text" e.g. {@joined}
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\}', '', text)

    # 4. Links/Formatting [[text|target]] -> text
    text = re.sub(r'\[\[([^|\]]+)(?:\|[^\]]+)?\]\]', r'\1', text)

    # 5. Remove English names in square brackets [English Name]
    # Be careful not to match random brackets in text like [1d6].
    # This regex targets brackets containing primarily latin characters.
    text = re.sub(r'\s*\[[a-zA-Z\s\'’\-]+\]', '', text)

    return text.strip()

def format_description_entry(entry):
    """Recursively format description entries (strings, lists, objects)"""
    if isinstance(entry, str):
        return clean_text(entry)
    elif isinstance(entry, list):
        return "<br>".join([format_description_entry(e) for e in entry])
    elif isinstance(entry, dict):
        # Handle complex entries with headers/lists inside
        title = ""
        if "name" in entry:
            title = f"<b>{clean_text(entry['name'])}.</b> "
        
        body = ""
        if "entries" in entry:
            body = format_description_entry(entry["entries"])
        elif "items" in entry:
            body = format_description_entry(entry["items"])
        elif "list" in entry:
             body = "<ul>" + "".join([f"<li>{format_description_entry(i)}</li>" for i in entry["list"]]) + "</ul>"
            
        return f"{title}{body}"
    return ""

def process_creature(creature_data):
    """Extracts relevant fields from the bestiary API response."""
    
    # Basic Info
    name = creature_data.get("name", {}).get("rus", "")
    name_eng = creature_data.get("name", {}).get("eng", "")
    
    # Header contains Type/Alignment (e.g., "Средняя нежить, нейтральная злая")
    # Sometimes it's in creature_data['header'] directly? Screenshot shows 'header' as string.
    creature_type = creature_data.get("header", "")
    
    # Source
    source_obj = creature_data.get("source", {})
    source_label = source_obj.get("shortName") or source_obj.get("name", {}).get("label") or "Unknown"
    
    # Stats
    # AC Processing
    # This block handles complex AC structures (sometimes list, sometimes dict)
    ac_data = creature_data.get("ac", [])
    ac_result_parts = []
    
    # Normalize to list
    if not isinstance(ac_data, list):
        ac_data = [ac_data]
        
    for entry in ac_data:
        if isinstance(entry, dict):
             # 1. Start with base AC value
             val = str(entry.get("ac", ""))
             
             # 2. Check for "special" override (e.g. "+ level")
             if "special" in entry:
                 val = entry["special"]
             
             # 3. Append sources (e.g. "natural armor")
             extras = []
             fro = entry.get("from", [])
             if fro:
                 if isinstance(fro, list): extras.extend(fro)
                 else: extras.append(str(fro))
                 
             cond = entry.get("condition")
             if cond: extras.append(cond)
             
             if extras and val:
                 val += f" ({', '.join(extras)})"
             
             ac_result_parts.append(val)
        else:
             # Just a number or string
             ac_result_parts.append(str(entry))
             
    ac = ", ".join([p for p in ac_result_parts if p])

    # HP Processing
    hit_data = creature_data.get("hit", {})
    hp_parts = []
    
    if isinstance(hit_data, dict):
        # 1. Base value (average or direct hit value)
        base_val = hit_data.get("average") or hit_data.get("hit")
        if base_val: 
            hp_parts.append(str(base_val))
        
        # 2. Text (e.g. "+ 10 per level")
        text = hit_data.get("text", "")
        if text:
            clean_t = clean_text(text).strip()
            # If text is an addition, append it. If no base value, use text entirely.
            if clean_t.startswith("+") or clean_t.startswith("-"):
                 hp_parts.append(clean_t)
            elif not base_val:
                 hp_parts.append(clean_t)
            
        # 3. Formula (only if text didn't cover it)
        formula = hit_data.get("formula", "")
        if formula and not text:
             hp_parts.append(f"({formula})")
             
        hp = " ".join(hp_parts)
    else:
        hp = str(hit_data)

    speed = creature_data.get("speed", "")
    
    # Abilities
    abilities = creature_data.get("abilities", {})
    stats = {
        "str": abilities.get("str", {}).get("value", 10),
        "dex": abilities.get("dex", {}).get("value", 10),
        "con": abilities.get("con", {}).get("value", 10),
        "int": abilities.get("int", {}).get("value", 10),
        "wis": abilities.get("wis", {}).get("value", 10),
        "cha": abilities.get("chr", {}).get("value", 10) # API uses 'chr' in screenshot
    }
    
    # Details
    senses = creature_data.get("sense", "")
    languages = creature_data.get("languages", "")
    
    challenge = str(creature_data.get("cr", ""))
    exp = creature_data.get("exp", "")
    if exp:
        challenge += f" ({exp} XP)"
        
    # Traits
    traits = []
    raw_traits = creature_data.get("traits", [])
    if raw_traits:
        for t in raw_traits:
            t_name = t.get("name", {}).get("rus", "")
            t_desc = format_description_entry(t.get("description", ""))
            traits.append({"name": t_name, "text": t_desc})

    # Actions
    actions = []
    raw_actions = creature_data.get("actions", [])
    if raw_actions:
         for a in raw_actions:
            a_name = a.get("name", {}).get("rus", "")
            a_desc = format_description_entry(a.get("description", ""))
            actions.append({"name": a_name, "text": a_desc})

    # Legendary
    legendary = []
    legendary_obj = creature_data.get("legendary", {})
    if legendary_obj:
        raw_legendary = legendary_obj.get("actions", [])
        if raw_legendary:
             for l in raw_legendary:
                l_name = l.get("name", {}).get("rus", "")
                l_desc = format_description_entry(l.get("description", ""))
                legendary.append({"name": l_name, "text": l_desc})

    # Main Description (Lore)
    description = format_description_entry(creature_data.get("description", ""))

    return {
        "name": name,
        "name_eng": name_eng,
        "type": creature_type,
        "source": source_label,
        "ac": ac,
        "hp": hp,
        "speed": speed,
        "stats": stats,
        "senses": senses,
        "languages": languages,
        "challenge": challenge,
        "traits": traits,
        "actions": actions,
        "legendary_actions": legendary,
        "description": description
    }

# ======================================================================================
# MAIN FETCH LOGIC
# ======================================================================================
def fetch_all_creatures():
    all_slugs = set()
    ordered_slugs = []
    page = 0
    limit = 50 
    
    print("Starting download list...")
    
    while True:
        payload = {
            "page": page,
            "limit": limit,
            "search": {
                "value": "",
                "exact": False
            },
            "order": {
                "field": "name",
                "direction": "asc"
            }
        }
        
        try:
            resp = requests.post(SEARCH_URL, json=payload, headers=HEADERS)
            if resp.status_code != 200:
                print(f"Error {resp.status_code}: {resp.text}")
                break
                
            data = resp.json()
            
            items = []
            if isinstance(data, list):
                items = data
            elif isinstance(data, dict):
                 items = data.get("items", [])
            
            if not items:
                print("No more items found (empty list).")
                break
                
            new_items_count = 0
            for item in items:
                slug = item.get("url", "")
                if slug and slug not in all_slugs:
                    all_slugs.add(slug)
                    ordered_slugs.append(slug)
                    new_items_count += 1
            
            print(f"Page {page}: Received {len(items)} items. New unique items: {new_items_count}. Total unique: {len(all_slugs)}")
            
            # Stop if the server returns items but we've seen all of them (looping same page)
            if new_items_count == 0:
                print("No new items found on this page. Stopping.")
                break

            # Stop if we received fewer items than the limit (end of list)
            # Note: If the server returns ALL items (611) despite limit=50, this logic handles it too (next loop will yield 0 new items).
            if len(items) < limit:
                 print("Received fewer items than limit. Stopping.")
                 break

            page += 1
            time.sleep(0.5) 
            
        except Exception as e:
            print(f"Exception: {e}")
            import traceback
            traceback.print_exc()
            break

    return ordered_slugs

def fetch_details_and_save(slugs):
    final_data = []
    total = len(slugs)
    
    print(f"Fetching details for {total} creatures...")
    
    for i, slug in enumerate(slugs):
        url = DETAILS_URL_TEMPLATE.format(slug=slug)
        attempts = 0
        success = False
        
        while attempts < 3 and not success:
            try:
                resp = requests.get(url, headers=HEADERS)
                if resp.status_code == 200:
                    data = resp.json()
                    processed = process_creature(data)
                    final_data.append(processed)
                    success = True
                elif resp.status_code == 429:
                    print("Rate limit (429). Waiting 5s...")
                    time.sleep(5)
                else:
                    print(f"Failed to fetch {slug}: {resp.status_code}")
                    break
            except Exception as e:
                print(f"Error fetching {slug}: {e}")
                
            attempts += 1
            time.sleep(0.1) # Be nice
            
        if (i+1) % 10 == 0:
            print(f"Processed {i+1}/{total}")
            # Intermediate save
            with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
                json.dump(final_data, f, ensure_ascii=False, indent=4)

    return final_data

if __name__ == "__main__":
    slugs = fetch_all_creatures()
    if slugs:
        full_data = fetch_details_and_save(slugs)
        
        with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
            json.dump(full_data, f, ensure_ascii=False, indent=4)
            
        print(f"Done! Saved {len(full_data)} creatures to {OUTPUT_FILE}")
    else:
        print("No creatures found.")

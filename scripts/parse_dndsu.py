import requests
from bs4 import BeautifulSoup
import json
import time
import os
import re

BASE_URL = "https://dnd.su"
RACES_URL = "https://dnd.su/race/"
CLASSES_URL = "https://dnd.su/class/"

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"
}

def clean_text(text):
    if not text: return ""
    return re.sub(r'\s+', ' ', text).strip()

def parse_races():
    print(f"Fetching races from {RACES_URL}...")
    response = requests.get(RACES_URL, headers=HEADERS)
    soup = BeautifulSoup(response.content, 'html.parser')
    
    races_links = []
    # Select specific race links to avoid homebrew garbage if possible, 
    # but for now we take generic grid items or list items.
    # inspecting dnd.su structure via common patterns:
    # Usually links are in lists under specific headers.
    
    # We will look for links that look like /race/ID-NAME/
    for a in soup.find_all('a', href=True):
        href = a['href']
        if href.startswith('/race/') or href.startswith('/multiverse/race/'):
            # Filter out generic links or "create new"
            if href.count('/') >= 3: 
                full_url = BASE_URL + href
                races_links.append(full_url)
    
    # Deduplicate
    races_links = list(set(races_links))
    print(f"Found {len(races_links)} potential races.")
    
    # Process all
    print(f"Processing all {len(races_links)} races...")
    
    races_data = []
    
    for url in races_links:
        print(f"Parsing {url}...")
        try:
            r_resp = requests.get(url, headers=HEADERS)
            r_soup = BeautifulSoup(r_resp.content, 'html.parser')
            
            race = {}
            # Initialize defaults
            race['speed'] = 30
            race['size'] = "Medium"
            race['asi'] = {"str": 0, "dex": 0, "con": 0, "int": 0, "wis": 0, "cha": 0}
            race['traits'] = {}
            race['languages'] = []

            # --- 1. NAME PARSING ---
            # Strategy: Use <title> tag. Format is usually "Name [Eng] - Race - DnD.su"
            # or "Name - Race - DnD.su"
            page_title = r_soup.title.string if r_soup.title else ""
            if page_title:
                # Example: "Ааракокра [Aarakocra] - Расы и происхождения - DnD.su"
                # Clean the title more aggressively
                temp = page_title
                
                # Split by '/' first (category)
                temp = temp.split('/')[0]
                # Split by '|' (sometimes used)
                temp = temp.split('|')[0]
                
                # Split by '[' (English name)
                temp = temp.split('[')[0]
                
                # Split by em-dash (—)
                temp = temp.split('—')[0]
                
                # Split by hyphen if surrounded by spaces " - "
                if ' - ' in temp:
                    temp = temp.split(' - ')[0]
                
                if not temp.strip(): # If we accidentally removed everything
                    temp = page_title

                race['name'] = temp.strip()
            else:
                # Fallback to H2
                h2 = r_soup.find('h2', class_='card-title')
                if h2: 
                     text = h2.get_text()
                     if '[' in text: race['name'] = text.split('[')[0].strip()
                     else: race['name'] = text.strip()
                else:
                    race['name'] = "Unknown Race"

            print(f"  -> Found race: {race['name']}")

            # --- 2. CONTENT PARSING ---
            # Try BEM style first (.card__body), then Bootstrap (.card-body)
            content_div = r_soup.find('div', class_='card__body')
            if not content_div:
                content_div = r_soup.find('div', class_='card-body')
            if not content_div:
                content_div = r_soup.find('div', class_='article-body') 
            if not content_div:
                # If still nothing, try to find a div with class 'desc' or similar?
                # Or just print warning and skip to avoid garbage
                print(f"  Warning: No content div found for {url}")
                continue # Skip this race to avoid garbage data

            # --- 3. TRAITS & STATS ---
            current_trait_name = "Description"
            current_trait_text = []
            
            # Keywords to ignore (Lore/Fluff)
            LORE_HEADERS = [
                "Имена", "Мужские имена", "Женские имена", "Прозвища", "Кланы", 
                "Общество", "История", "Отыгрыш", "Почему ", "Редкость", "Изгой", 
                "Друзья", "Разновидности", "Наследие", "Приключения", "В мире", 
                "Жизнь", "Культура", "Верования", "Отношения", "Характер",
                "Боги", "Города", "Земли", "Поселения", "Традиции"
            ]

            def is_lore(header):
                header_lower = header.lower()
                for k in LORE_HEADERS:
                    if k.lower() in header_lower:
                        return True
                # Extra check for phrases like "Orcs and Humans"
                if " и " in header_lower and len(header.split()) > 2 and header[0].isupper():
                     # Heuristic: headers with "and" often describe relationships, unless it's a specific feature
                     pass 
                return False

            # Helper to commit a trait
            def pack_trait_final(name, text_list):
                full_text = " ".join(text_list).strip()
                if not full_text: return
                
                # Clean name
                clean_name = name.strip().rstrip('.:')
                
                # Filter lore headers
                bad_headers = [
                    "Имена", "Мужские", "Женские", "Прозвища", "Кланы", "Общество", "История", 
                    "Отыгрыш", "Почему ", "Редкость", "Изгой", "Друзья", "Разновидности", 
                    "Наследие", "Приключения", "В мире", "Жизнь", "Культура", "Верования", 
                    "Отношения", "Характер", "Боги", "Города", "Земли", "Поселения", 
                    "Традиции", "Долгая память", "Чужие в странной", "Галерея", "Комментрии", 
                    "Источник", "Show/Hide", "Внешний вид"
                ]
                for bad in bad_headers:
                    if bad.lower() in clean_name.lower():
                        return

                # --- ASI Parsing ---
                stats_map = {
                    "Сила": "str", "Силы": "str", "Силу": "str",
                    "Ловкость": "dex", "Ловкости": "dex",
                    "Телосложение": "con", "Телосложения": "con",
                    "Интеллект": "int", "Интеллекта": "int",
                    "Мудрость": "wis", "Мудрости": "wis",
                    "Харизма": "cha", "Харизмы": "cha"
                }

                # Look for "Ability Score Increase" or similar patterns
                if "увеличивается на" in full_text or "значения характеристик" in full_text:
                    sentences = re.split(r'[.;]', full_text)
                    for s in sentences:
                        # Case 1: "Сила ... увеличивается на 2"
                        if "увеличивается на" in s:
                             for ru_stat, en_key in stats_map.items():
                                 if ru_stat in s:
                                     # Look for number after "увеличивается на"
                                     m = re.search(r'увеличивается на\s*(\d+)', s)
                                     if m:
                                         val = int(m.group(1))
                                         race['asi'][en_key] = max(race['asi'][en_key], val)
                        
                        # Case 2: "значения характеристик увеличиваются на 1" (Human)
                        if "значения характеристик" in s and "на 1" in s:
                             for k in race['asi']:
                                 race['asi'][k] = max(race['asi'][k], 1)

                # --- SPEED ---
                if "скорость" in clean_name.lower() or ("скорость" in full_text.lower() and len(full_text) < 200):
                     m = re.search(r'(\d+)\s*фут', full_text)
                     if m: race['speed'] = int(m.group(1))

                # --- SIZE ---
                if "размер" in clean_name.lower() or ("размер" in full_text.lower() and len(full_text) < 200):
                    if "Средний" in full_text: race['size'] = "Средний"
                    elif "Маленький" in full_text: race['size'] = "Маленький"

                # Store trait
                race['traits'][clean_name] = full_text


            # Iterate through children elements
            for tag in content_div.find_all(['p', 'li', 'h3', 'h4']):
                text = clean_text(tag.get_text())
                if not text: continue
                if text in ["Описание", "Show/Hide"]: continue
                
                # Check for "<strong>Name.</strong> Body" pattern inside <p>
                strong = tag.find('strong')
                if strong:
                    bold_text = clean_text(strong.get_text())
                    # If the bold text ends with dot and is at start
                    if bold_text.endswith('.') or bold_text.endswith(':'):
                        # Commit previous
                        pack_trait_final(current_trait_name, current_trait_text)
                        
                        # New trait
                        current_trait_name = bold_text.rstrip('.:')
                        # The rest of the paragraph is the text
                        remain_text = text.replace(bold_text, "", 1).strip()
                        current_trait_text = [remain_text]
                        continue
                
                # Check for Headers <h3> <h4>
                if tag.name in ['h3', 'h4']:
                    # Commit previous
                    pack_trait_final(current_trait_name, current_trait_text)
                    current_trait_name = text
                    current_trait_text = []
                else:
                    # Just text paragraph, append to current trait
                    current_trait_text.append(text)
            
            # Pack the final accumulation
            pack_trait_final(current_trait_name, current_trait_text)
            
            # Description cleanup
            if "Description" in race['traits']:
                 race['description'] = race['traits'].pop("Description")
            
            # Clean up keys
            keys_to_remove = []
            for k in race['traits']:
                 if "На данный момент в галерее" in k: keys_to_remove.append(k)
                 if k == "DnD.su": keys_to_remove.append(k)
            for k in keys_to_remove: 
                del race['traits'][k]
            
            if 'name' in race:
                races_data.append(race)

                
            time.sleep(0.5) # Be polite
            
        except Exception as e:
            print(f"Error parsing {url}: {e}")
            
    with open('races_dndsu.json', 'w', encoding='utf-8') as f:
        json.dump(races_data, f, ensure_ascii=False, indent=2)
    print("Saved races_dndsu.json")

def parse_classes():
    print(f"Fetching classes from {CLASSES_URL}...")
    try:
        response = requests.get(CLASSES_URL, headers=HEADERS)
        soup = BeautifulSoup(response.content, 'html.parser')
        
        classes_links = []
        for a in soup.find_all('a', href=True):
            href = a['href']
            if href.startswith('/class/'):
                # Avoid subpages like /class/bard/spells/
                # '/class/ID-NAME/' -> strip -> 'class/ID-NAME' -> count('/') = 1
                if href.strip('/').count('/') == 1: 
                    full_url = BASE_URL + href
                    classes_links.append(full_url)
        
        classes_links = list(set(classes_links))
        print(f"Found {len(classes_links)} potential classes.")
        
        classes_data = []
        
        for url in classes_links:
            print(f"Parsing class {url}...")
            try:
                r_resp = requests.get(url, headers=HEADERS)
                r_soup = BeautifulSoup(r_resp.content, 'html.parser')
                
                cls = {
                    "name": "",
                    "description": "",
                    "hitDie": None,
                    "primaryAbility": "",
                    "savingThrowProficiencies": None,
                    "armorProficiencies": None,
                    "weaponProficiencies": None
                }
                
                # Name Parsing
                page_title = r_soup.title.string if r_soup.title else ""
                if page_title:
                    # Reuse cleaning logic: Split by delimiter
                    temp = page_title.split('/')[0].split('|')[0].split('[')[0].split('—')[0]
                    if ' - ' in temp: temp = temp.split(' - ')[0]
                    cls['name'] = temp.strip()
                else:
                    cls['name'] = "Unknown Class"
                
                print(f"  -> Found class: {cls['name']}")
                
                # Content Parsing
                # Find body using known identifiers
                content_div = None
                for selector in ['.card__body', '.card-body', '.article-body', '.content']:
                    content_div = r_soup.select_one(selector)
                    if content_div: break
                
                if not content_div:
                    print(f"  Warning: No content div found for {url}!")
                    content_div = r_soup # Fallback, but risky
                
                # Parse specific fields from <p> tags starting with <strong>
                # E.g. <p><strong>Доспехи:</strong> Лёгкие доспехи</p>
                
                for tag in content_div.find_all(['p', 'li']):
                    # Check if tag starts with strong/b
                    first_bold = tag.find(['strong', 'b'])
                    if not first_bold: continue
                    
                    header_text = clean_text(first_bold.get_text()).lower()
                    full_p_text = clean_text(tag.get_text())
                    
                    # Remove the header from the text to get the value
                    # Heuristic: the header usually ends with ':'
                    # If so, split by ':'
                    if ':' in full_p_text:
                        value_text = full_p_text.split(':', 1)[1].strip()
                    else:
                        # Fallback: remove the bold text
                        value_text = full_p_text.replace(clean_text(first_bold.get_text()), "", 1).strip()
                    
                    # Strict Clean Header
                    h_clean = header_text.rstrip(':').strip()

                    # -- HIT DIE --
                    if h_clean == "кость хитов" and cls['hitDie'] is None:
                        m_hd = re.search(r'1к(\d+)', value_text)
                        if m_hd:
                            cls['hitDie'] = int(m_hd.group(1))

                    # -- ARMOR --
                    elif h_clean == "доспехи" and cls['armorProficiencies'] is None:
                        # Value: "Лёгкие доспехи, средние доспехи, щиты"
                        # Clean logic: remove "Нет" if present
                        if "нет" in value_text.lower() and len(value_text) < 5:
                            cls['armorProficiencies'] = []
                        else:
                            cls['armorProficiencies'] = [x.strip() for x in value_text.split(',') if x.strip()]

                    # -- WEAPONS --
                    elif h_clean == "оружие" and cls['weaponProficiencies'] is None:
                        if "нет" in value_text.lower() and len(value_text) < 5:
                            cls['weaponProficiencies'] = []
                        else:
                            cls['weaponProficiencies'] = [x.strip() for x in value_text.split(',') if x.strip()]

                    # -- SAVING THROWS --
                    elif h_clean == "спасброски" and cls['savingThrowProficiencies'] is None:
                         cls['savingThrowProficiencies'] = [x.strip() for x in value_text.split(',') if x.strip()]
                         if cls['savingThrowProficiencies']:
                             cls['primaryAbility'] = cls['savingThrowProficiencies'][0]

                # Fill defaults if nothing found
                if cls['hitDie'] is None: cls['hitDie'] = 8
                if cls['armorProficiencies'] is None: cls['armorProficiencies'] = []
                if cls['weaponProficiencies'] is None: cls['weaponProficiencies'] = []
                if cls['savingThrowProficiencies'] is None: cls['savingThrowProficiencies'] = []

                classes_data.append(cls)
                time.sleep(0.5)

            except Exception as e:
                print(f"Error parsing class {url}: {e}")
        
        with open('classes_dndsu.json', 'w', encoding='utf-8') as f:
            json.dump(classes_data, f, ensure_ascii=False, indent=2)
        print("Saved classes_dndsu.json")
    
    except Exception as e:
        print(f"Error collecting classes: {e}")

if __name__ == "__main__":
    parse_races()
    parse_classes()

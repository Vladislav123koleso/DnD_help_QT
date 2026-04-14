import requests
import json
import time
import re
import os

# ======================================================================================
# КУКИ (Проверенные)
# ======================================================================================
MY_COOKIE = "_ga_WXD97L1HCX=GS2.1.s1751057246$o8$g0$t1751057246$j60$l0$h0; _ga=GA1.1.119112224.1730987303; _ym_uid=1730987305825672626; _ym_d=1746887959; ttg-theme=dark"

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept": "application/json, text/plain, */*",
    "Content-Type": "application/json",
    "Origin": "https://new.ttg.club",
    "Referer": "https://new.ttg.club/items",
    "Cookie": MY_COOKIE
}

SEARCH_URL = "https://new.ttg.club/api/v2/item/search"
DETAILS_URL_TEMPLATE = "https://new.ttg.club/api/v2/item/{slug}"

def clean_text(text):
    if not text or text == "None": return ""
    
    # Ensure text is string
    if not isinstance(text, str): text = str(text)

    # 1. Complex tags with parameters {@tag text|params} -> text
    # Используем DOTALL чтобы захватывать переносы строк внутри тега
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\s+(.*?)\|.*?\}', r'\1', text, flags=re.DOTALL) 
    
    # 2. Simple tags {@tag text} -> text
    # The previous regex missed cases where there is no space, or Russian text immediately follows, 
    # or where the params pipe is missing but it's still a tag structure.
    # Improved: catch {@tag text_with_optional_stuff}
    # Also catch cases like {@glossary text (clarification)}
    
    # Case A: {@tag text}
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\s+([^}]*?)\}', r'\1', text, flags=re.DOTALL)

    # Case B: {@tag} without arguments (sometimes used as marker) -> remove
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\}', '', text)
    
    # Case C: Broken/Partial tags like {@glossary text without closing brace if it happens (less safe, but let's try specific ones)
    # Based on user screenshot: {@glossary ... without closing brace? Or maybe just ensure we catch all {@...} patterns.
    
    # 3. Last resort: remove any remaining {@tag markers that might have leaked if regex didn't match perfectly
    # For example "{@glossary word" where 'word' is just text. We just want to remove "{@glossary ".
    text = re.sub(r'\{@[a-zA-Z0-9_-]+\s+', '', text) 

    # 4. Remove [Source] brackets if they are just refs
    text = re.sub(r'\s*\[.*?\]', '', text) 
    
    # 5. Fix "Header:}" typo common in TTG
    text = re.sub(r'([А-Яа-яЁё]+):\s*\}', r'\1:', text)

    # 6. Cleaning remaining braces if any
    text = text.replace('}', '')

    return text.strip()

def format_cost(cost_data):
    if not cost_data: return ""
    # Если строка (как "25 зм") - возвращаем как есть
    if isinstance(cost_data, str): return cost_data
    
    if isinstance(cost_data, dict):
        mapping = {'gp':'зм', 'sp':'см', 'cp':'мм', 'pp':'пм', 'ep':'эм'}
        parts = []
        for k, v in cost_data.items():
            unit = mapping.get(k, k)
            parts.append(f"{v} {unit}")
        return ", ".join(parts)
    return str(cost_data)

def process_description_entry(entry):
    if isinstance(entry, str):
        return clean_text(entry)
    elif isinstance(entry, dict):
        if entry.get('type') == 'table':
            # Build HTML Table
            html = "<br>"
            if 'caption' in entry:
                html += f"<b>{clean_text(entry['caption'])}</b>"
            
            html += "<table border='1' cellspacing='0' cellpadding='4' width='100%'>"
            
            # Headers
            if 'colLabels' in entry:
                html += "<thead><tr>"
                for h in entry['colLabels']:
                    html += f"<th bgcolor='#2d2d2d'>{clean_text(str(h))}</th>" 
                html += "</tr></thead>"
            
            # Rows
            if 'rows' in entry:
                html += "<tbody>"
                for row in entry['rows']:
                    html += "<tr>"
                    for cell in row:
                        val = cell
                        if isinstance(cell, dict) and 'text' in cell: val = cell['text']
                        elif isinstance(cell, dict) and 'roll' in cell: 
                            val = f"{cell.get('min')}..{cell.get('max')}"
                        
                        html += f"<td>{clean_text(str(val))}</td>"
                    html += "</tr>"
                html += "</tbody>"
            
            html += "</table><br>"
            return html
            
        elif 'entries' in entry:
            sub_entries = entry['entries']
            sub_text = []
            for sub in sub_entries:
                res = process_description_entry(sub)
                if res: sub_text.append(res)
            return "<br>".join(sub_text)
        
        elif 'text' in entry:
            return clean_text(entry['text'])
    
    return ""

def run():
    print(f"--- ПАРСИНГ ПРЕДМЕТОВ (TTG) ---")
    print(f"URL поиска: {SEARCH_URL}")
    
    slugs = set()
    alphabet = 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'
    
    # 1. СБОР ССЫЛОК (SLUGS)
    print("Сбор списка предметов...")
    for char in alphabet:
        try:
            resp = requests.post(SEARCH_URL, json={"query": char}, headers=HEADERS)
            if resp.status_code == 200:
                data = resp.json()
                # Обычно приходит список или dict c ключом items
                items_list = data if isinstance(data, list) else data.get('items', data.get('data', []))
                
                count_ok = 0
                for item in items_list:
                    # Бывает slug, бывает url
                    slug = item.get('url') or item.get('slug')
                    if slug:
                        slugs.add(slug)
                        count_ok += 1
                print(f"Буква '{char}': найдено {count_ok} шт.")
            else:
                print(f"Буква '{char}': ошибка {resp.status_code}")
        except Exception as e:
            print(f"Буква '{char}': исключение {e}")
            
        time.sleep(0.2) # Небольшая пауза

    print(f"\nВсего уникальных предметов: {len(slugs)}")
    if len(slugs) == 0:
        print("Ничего не найдено. Проверьте куки или доступность сайта.")
        return

    # 3. ПОДГОТОВКА (ПЕРЕЗАПИСЬ ИЛИ ДОЗАПИСЬ)
    # Сейчас мы сделаем умный режим: если файл есть, читаем его
    # Но так как мы хотим гарантированно 100%, проще пройтись по всем 293 и проверить.
    # Для простоты - перезапишем, но с НАДЕЖНЫМ повтором при ошибках.
    
    full_items_db = []
    print("\nСкачивание деталей... (с защитой от сбоев 429)")
    
    sorted_slugs = sorted(list(slugs))
    
    for i, raw_slug in enumerate(sorted_slugs):
        clean_slug = raw_slug.strip('/').split('/')[-1]
        url = DETAILS_URL_TEMPLATE.format(slug=clean_slug)
        
        # --- ПОПЫТКИ СКАЧИВАНИЯ (RETRY LOOP) ---
        max_retries = 5
        success = False
        
        for attempt in range(max_retries):
            try:
                r = requests.get(url, headers=HEADERS)
                
                if r.status_code == 200:
                    d = r.json()
                    
                    # --- ПАРСИНГ ---
                    name = d.get('name', {}).get('rus', '???')
                    eng_name = d.get('name', {}).get('eng', '')
                    
                    # Описание
                    desc_raw = d.get('description', '')
                    desc_text = ""
                    if isinstance(desc_raw, list):
                        lines = []
                        for el in desc_raw:
                            res = process_description_entry(el)
                            if res: lines.append(res)
                        desc_text = "<br><br>".join(lines)
                    else:
                        desc_text = clean_text(str(desc_raw))
                    
                    # Цена
                    cost_str = format_cost(d.get('cost'))
                    
                    # Тип (field 'type' OR 'types')
                    type_obj = d.get('type')
                    if not type_obj: type_obj = d.get('types') # Fallback to plural field
                    
                    type_str = ""
                    if isinstance(type_obj, dict):
                        type_str = type_obj.get('name', '')
                    elif isinstance(type_obj, str):
                        type_str = type_obj
                    
                    # Редкость
                    rarity = d.get('rarity')
                    if rarity:
                        rarity_str = rarity.get('name', '') if isinstance(rarity, dict) else str(rarity)
                    else:
                        # Если редкости нет, то для обычных предметов это "Обычный"
                        rarity_str = "Обычный"
                    
                    # Вес
                    weight = str(d.get('weight', ''))
                    
                    # Источник
                    source = d.get('source', {})
                    source_str = source.get('name', {}).get('rus', '') if isinstance(source, dict) and isinstance(source.get('name'), dict) else ''

                    item_entry = {
                        "name": name,
                        "name_eng": eng_name,
                        "type": type_str,
                        "rarity": rarity_str,
                        "cost": cost_str,
                        "weight": weight,
                        "source": source_str,
                        "description": desc_text
                    }
                    
                    full_items_db.append(item_entry)
                    success = True
                    break # Выход из цикла повторов
                
                elif r.status_code == 429:
                    wait_time = 5 + (attempt * 2) # 5, 7, 9...
                    print(f"⚠️ 429 (Слишком часто). Ждем {wait_time} сек и пробуем снова ({attempt+1}/{max_retries})...")
                    time.sleep(wait_time)
                    continue # Пробуем снова этот же предмет
                
                else:
                    print(f"❌ Ошибка {r.status_code} для {clean_slug}")
                    break # Другая ошибка - пропускаем
                    
            except Exception as e:
                print(f"❌ Исключение {e} для {clean_slug}")
                time.sleep(1)
        
        if not success:
            print(f"💀 Не удалось скачать: {clean_slug}")

        # Логирование
        if (i+1) % 10 == 0 or (i+1) == len(sorted_slugs):
            print(f"[{i+1}/{len(slugs)}] Обработан: {full_items_db[-1]['name'] if success else 'ОШИБКА'}")
        
        # Пауза между предметами (немного увеличим для стабильности)
        time.sleep(0.4)

    # 3. СОХРАНЕНИЕ
    output_path = os.path.join(os.path.dirname(__file__), 'final_item.json')
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(full_items_db, f, ensure_ascii=False, indent=4)
        
    print(f"\nГотово! Сохранено {len(full_items_db)} предметов в: {output_path}")

if __name__ == "__main__":
    run()
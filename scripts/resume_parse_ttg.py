import requests
import json
import time
import os

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept": "application/json"
}

search_url = "https://new.ttg.club/api/v2/spells/search"
DETAILS_URL_TEMPLATE = "https://new.ttg.club/api/v2/spells/{slug}"
OUTPUT_FILE = 'final_spells.json'

def run():
    # 1. Загружаем уже скачанные заклинания
    existing_spells = []
    existing_names = set()
    
    if os.path.exists(OUTPUT_FILE):
        try:
            with open(OUTPUT_FILE, 'r', encoding='utf-8') as f:
                existing_spells = json.load(f)
                # Собираем имена уже скачанных, чтобы не дублировать
                # Но лучше ориентироваться по slug, однако в final_spells.json мы slug не сохраняли.
                # Поэтому будем ориентироваться по русскому имени.
                for s in existing_spells:
                    existing_names.add(s['name'])
            print(f"Загружено {len(existing_spells)} уже скачанных заклинаний.")
        except Exception as e:
            print(f"Ошибка чтения существующего файла: {e}")
            existing_spells = []

    # 2. Собираем полный список slug с сайта
    slugs_map = {} # slug -> name (для проверки)
    alphabet  = 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'

    print("Обновляем список всех заклинаний с сайта...")
    for char in alphabet:
        try:
            response = requests.post(search_url, json={'query': char}, headers=HEADERS)
            if response.status_code == 200:
                data = response.json()
                for item in data:
                    if 'url' in item and 'name' in item:
                        slugs_map[item['url']] = item['name']['rus']
            time.sleep(0.2)
        except Exception as e:
            print(f"Ошибка поиска '{char}': {e}")

    print(f"Всего на сайте найдено: {len(slugs_map)}")

    # 3. Вычисляем, что нужно докачать
    # Проверяем по имени, так как slug мы не сохраняли в файл
    slugs_to_download = []
    for slug, name in slugs_map.items():
        if name not in existing_names:
            slugs_to_download.append(slug)

    if not slugs_to_download:
        print("Все заклинания уже скачаны! Ничего делать не нужно.")
        return

    print(f"Нужно докачать: {len(slugs_to_download)} шт.")

    # 4. Докачиваем
    new_spells = []
    
    for i, slug in enumerate(slugs_to_download):
        url = DETAILS_URL_TEMPLATE.format(slug=slug)
        try:
            resp = requests.get(url, headers=HEADERS)
            
            if resp.status_code == 200:
                spell_data = resp.json()
                if spell_data:
                    # --- ОБРАБОТКА (копия логики из основного скрипта) ---
                    desc_raw = spell_data.get('description', '')
                    desc_parts = []
                    if isinstance(desc_raw, list):
                        for part in desc_raw:
                            if isinstance(part, str): desc_parts.append(part)
                            elif isinstance(part, dict): desc_parts.append(part.get('text', str(part)))
                            else: desc_parts.append(str(part))
                        desc = "\n".join(desc_parts)
                    else:
                        desc = str(desc_raw)

                    comps_obj = spell_data.get('components', {})
                    comps_list = []
                    if comps_obj.get('v'): comps_list.append('В')
                    if comps_obj.get('s'): comps_list.append('С')
                    m_val = comps_obj.get('m')
                    if m_val:
                        if isinstance(m_val, str): comps_list.append(f'М ({m_val})')
                        else: comps_list.append('М')
                    components_str = ", ".join(comps_list)

                    classes_list = spell_data.get('classes', [])
                    class_names = []
                    if not classes_list:
                         aff = spell_data.get('affiliation', {})
                         classes_list = aff.get('classes', [])
                    for c in classes_list:
                        if isinstance(c, dict):
                            name_val = c.get('name')
                            if isinstance(name_val, dict) and 'rus' in name_val: class_names.append(name_val['rus'])
                            elif isinstance(name_val, str): class_names.append(name_val)
                        elif isinstance(c, str): class_names.append(c)
                    classes_str = ", ".join(class_names)

                    # Subclasses
                    aff = spell_data.get('affiliation', {})
                    subclasses_list = aff.get('subclasses', [])
                    subclass_names = []
                    for sc in subclasses_list:
                        if isinstance(sc, dict):
                            name_val = sc.get('name')
                            if isinstance(name_val, str): subclass_names.append(name_val)
                    subclasses_str = ", ".join(subclass_names)

                    # Source
                    source_obj = spell_data.get('source', {})
                    source_name = ""
                    if isinstance(source_obj, dict):
                        source_name_obj = source_obj.get('name', {})
                        if isinstance(source_name_obj, dict):
                            source_name = source_name_obj.get('rus', source_name_obj.get('label', ''))
                        elif isinstance(source_name_obj, str):
                            source_name = source_name_obj
                    
                    # Page
                    source_page = str(source_obj.get('page', ''))
                    if source_page and source_name:
                        source_full = f"{source_name} (стр. {source_page})"
                    else:
                        source_full = source_name

                    is_ritual = 1 if spell_data.get('ritual') else 0
                    
                    upper_list = spell_data.get('upper', [])
                    upper_str = ""
                    if isinstance(upper_list, list): upper_str = "\n".join(upper_list)
                    else: upper_str = str(upper_list)

                    clean_spell = {
                        "name": spell_data['name']['rus'],
                        "level": spell_data['level'],
                        "school": spell_data['school'],
                        "description": desc,
                        "ritual": is_ritual,
                        "castingTime": spell_data.get('castingTime', ''),
                        "range": spell_data.get('range', ''),
                        "duration": spell_data.get('duration', ''),
                        "components": components_str,
                        "classes": classes_str,
                        "subclasses": subclasses_str,
                        "source": source_full,
                        "upper": upper_str
                    }

                    new_spells.append(clean_spell)
                    print(f"[{i+1}/{len(slugs_to_download)}] + {clean_spell['name']}")
                else:
                    print(f"[{i+1}] Пустые данные: {slug}")
            
            elif resp.status_code == 429:
                print(f"[{i+1}] 429 Too Many Requests. Ждем 10 сек...")
                time.sleep(10)
                # Можно добавить логику повтора, но пока просто пропустим
            elif resp.status_code == 502:
                print(f"[{i+1}] 502 Bad Gateway. Ждем 5 сек...")
                time.sleep(5)
            else:
                print(f"[{i+1}] Ошибка {resp.status_code} для {slug}")

        except Exception as e:
            print(f"Ошибка: {e}")
        
        time.sleep(1.5) # Пауза

    # 5. Объединяем и сохраняем
    if new_spells:
        total_spells = existing_spells + new_spells
        print(f"\nСохраняем {len(total_spells)} заклинаний в файл...")
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
            json.dump(total_spells, f, ensure_ascii=False, indent=4)
        print("Готово!")
    else:
        print("\nНовых заклинаний не скачано.")

if __name__ == "__main__":
    run()

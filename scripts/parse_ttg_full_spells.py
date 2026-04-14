import requests
import json
import time

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept": "application/json"
}

search_url = "https://new.ttg.club/api/v2/spells/search"
DETAILS_URL_TEMPLATE = "https://new.ttg.club/api/v2/spells/{slug}"

def run():
    # 1. Собираем список
    slugs = set()
    alphabet  = 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'

    print("Начинаем поиск заклинаний...")

    for char in alphabet:
        print(f"Ищем на букву '{char}'...")
        try:
            response = requests.post(search_url, json={'query': char}, headers=HEADERS)
            if response.status_code == 200:
                data = response.json()
                for item in data:
                    if 'url' in item:
                        slugs.add(item['url'])
            time.sleep(0.5)
        except Exception as e:
            print(f"Ошибка поиска: {e}")

    # 2. Качаем детали
    print(f"\nНайдено {len(slugs)} заклинаний. Скачиваем подробности...")
    full_spells_db = []

    for i, slug in enumerate(slugs):
        url = DETAILS_URL_TEMPLATE.format(slug=slug)
        try:
            resp = requests.get(url, headers=HEADERS)
            if resp.status_code == 200:
                spell_data = resp.json()
                if spell_data:
                    # --- УЛУЧШЕННАЯ ОБРАБОТКА ОПИСАНИЯ ---
                    desc_raw = spell_data.get('description', '')
                    desc_parts = []
                    
                    if isinstance(desc_raw, list):
                        for part in desc_raw:
                            if isinstance(part, str):
                                desc_parts.append(part)
                            elif isinstance(part, dict):
                                # Если это объект (например ссылка), пытаемся найти текст
                                # Часто бывает {text: "...", url: "..."}
                                if 'text' in part:
                                    desc_parts.append(part['text'])
                                else:
                                    # Если непонятный объект, просто превращаем в строку
                                    desc_parts.append(str(part))
                            else:
                                desc_parts.append(str(part))
                        desc = "\n".join(desc_parts)
                    else:
                        desc = str(desc_raw)
                    # ---------------------------------------

                    # Компоненты
                    comps_obj = spell_data.get('components', {})
                    comps_list = []
                    if comps_obj.get('v'): comps_list.append('В')
                    if comps_obj.get('s'): comps_list.append('С')
                    m_val = comps_obj.get('m')
                    if m_val:
                        if isinstance(m_val, str):
                            comps_list.append(f'М ({m_val})')
                        else:
                            comps_list.append('М')
                    components_str = ", ".join(comps_list)

                    # Классы
                    classes_list = spell_data.get('classes', [])
                    class_names = [] 
                    if not classes_list:
                         aff = spell_data.get('affiliation', {})
                         classes_list = aff.get('classes', [])
                    for c in classes_list:
                        if isinstance(c, dict):
                            name_val = c.get('name')
                            if isinstance(name_val, dict) and 'rus' in name_val: 
                                class_names.append(name_val['rus'])
                            elif isinstance(name_val, str):
                                class_names.append(name_val)
                        elif isinstance(c, str):
                            class_names.append(c)
                    classes_str = ", ".join(class_names)

                    # Ритуал
                    is_ritual = 1 if spell_data.get('ritual') else 0
                    
                    # Upper
                    upper_list = spell_data.get('upper', [])
                    upper_str = ""
                    if isinstance(upper_list, list):
                        upper_str = "\n".join(upper_list)
                    else:
                        upper_str = str(upper_list)

                    clean_spell = {
                        "name": spell_data['name']['rus'],
                        "level": spell_data['level'],
                        "school": spell_data['school'],
                        "description": desc, # Используем обработанное описание
                        "ritual": is_ritual,
                        "castingTime": spell_data.get('castingTime', ''),
                        "range": spell_data.get('range', ''),
                        "duration": spell_data.get('duration', ''),
                        "components": components_str,
                        "classes": classes_str,
                        "upper": upper_str
                    }

                    full_spells_db.append(clean_spell)
                    print(f"[{i+1}/{len(slugs)}] + {clean_spell['name']}")
                else:
                    print(f"[{i+1}/{len(slugs)}] - Пустые данные: {slug}")
            elif resp.status_code == 429:
                print(f"[{i+1}/{len(slugs)}] - СЛИШКОМ БЫСТРО (429). Ждем 5 секунд...")
                time.sleep(5) # Если поймали блок, ждем дольше
            else:
                print(f"[{i+1}/{len(slugs)}] - Ошибка {resp.status_code} для '{slug}'")
        except Exception as e:
            print(f"[{i+1}/{len(slugs)}] - Ошибка парсинга {slug}: {e}")
        
        # УВЕЛИЧЕННАЯ ПАУЗА
        time.sleep(1.2) 

    # 3. Сохраняем
    with open('final_spells.json', 'w', encoding='utf-8') as f:
        json.dump(full_spells_db, f, ensure_ascii=False, indent=4)
    print("\nГотово! Файл final_spells.json создан.")

if __name__ == "__main__":
    run()

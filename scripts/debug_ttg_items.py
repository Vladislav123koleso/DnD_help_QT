import requests
import json
import time
import re

# ======================================================================================
# КУКИ ОТ ПОЛЬЗОВАТЕЛЯ (Актуальные)
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

DETAILS_URL_TEMPLATE = "https://new.ttg.club/api/v2/item/{slug}"

def run():
    print("--- ЗАПУСК ДИАГНОСТИКИ ПОИСКА ПРЕДМЕТОВ ---")
    
    simple_payload = {"query": "а"}
    complex_payload = {"page": 0, "limit": 10, "search": {"value": "а", "exact": False}}
    
    candidates = [
        ("api/v2/item", complex_payload, "Complex Item"),
        ("api/v2/item", simple_payload, "Simple Item"),
        ("api/v2/items/search", simple_payload, "Items Search"),
        ("api/v2/items", complex_payload, "Items Root"),
        ("api/v2/item/search", simple_payload, "Item Search"),
        ("api/v2/search", {"query": "а", "index": "item"}, "Global Search"),
    ]

    working_config = None

    for suffix, payload, name in candidates:
        url = f"https://new.ttg.club/{suffix}"
        print(f"[{name}] {url} ...", end=" ")
        
        try:
            time.sleep(0.5)
            resp = requests.post(url, json=payload, headers=HEADERS)
            
            if resp.status_code == 200:
                print(f"✅ OK! (200)")
                try:
                    data = resp.json()
                    lst = data if isinstance(data, list) else data.get('items', data.get('data', []))
                    if lst:
                        print(f"   -> Найдено {len(lst)} элементов. ЭТО ОНО!")
                        working_config = (url, payload)
                        break
                    else:
                        print(f"   -> Пустой список (но 200 OK).")
                        if not working_config: working_config = (url, payload)
                except:
                    print("   -> Не JSON ответ.")
            elif resp.status_code == 403: print("⛔ 403 (Forbidden)")
            elif resp.status_code == 500: print("❌ 500 (Server Error)")
            elif resp.status_code == 405: print("🚫 405 (Method Not Allowed)")
            else: print(f"⚠️ {resp.status_code}")
                
        except Exception as e:
            print(f"Err: {e}")

    if not working_config:
        print("\nНи один метод не дал 100% результата. Пробуем аварийный режим (перебор букв на api/v2/item)...")
        working_config = ("https://new.ttg.club/api/v2/item", simple_payload)
    
    target_url, target_payload_template = working_config
    print(f"\n--- ПАРСИНГ ЧЕРЕЗ: {target_url} ---")
    
    slugs = set()
    alphabet = 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'
    
    if "query" in target_payload_template and "page" not in target_payload_template:
        for char in alphabet:
            print(f"Буква '{char}'...", end=" ")
            try:
                resp = requests.post(target_url, json={"query": char}, headers=HEADERS)
                if resp.status_code == 200:
                    items = resp.json()
                    if isinstance(items, dict): items = items.get('items', items.get('data', []))
                    added = 0
                    for it in items:
                        u = it.get('url') or it.get('slug')
                        if u: slugs.add(u); added += 1
                    print(f"Найдено: {added}")
                else: print(f"Код {resp.status_code}")
            except Exception as e: print(f"Err {e}")
            time.sleep(0.5)
    else:
        page = 0
        while True:
            print(f"Cтраница {page}...", end=" ")
            payload = target_payload_template.copy()
            payload["page"] = page
            try:
                resp = requests.post(target_url, json=payload, headers=HEADERS)
                if resp.status_code == 200:
                    data = resp.json()
                    items = data.get('items', data.get('data', []))
                    if not items: break
                    count = 0
                    for it in items:
                        u = it.get('url') or it.get('slug')
                        if u: slugs.add(u); count += 1
                    print(f"OK (+{count})")
                    page += 1
                else: break
            except: break
            time.sleep(1.0)

    print(f"\nИтого уникальных ссылок: {len(slugs)}")
    
    full_items_db = []
    for i, raw_slug in enumerate(slugs):
        slug = raw_slug.strip('/').split('/')[-1]
        url = DETAILS_URL_TEMPLATE.format(slug=slug)
        try:
            r = requests.get(url, headers=HEADERS)
            if r.status_code == 200:
                d = r.json()
                name = d.get('name', {}).get('rus', '???')
                full_items_db.append({
                    "name": name,
                    "type": str(d.get('type', {}).get('name', '') if isinstance(d.get('type'), dict) else d.get('type', '')),
                })
                print(f"[{i+1}/{len(slugs)}] Скачан: {name}")
        except: pass
        time.sleep(0.3)
        
    with open('scripts/final_item.json', 'w', encoding='utf-8') as f:
        json.dump(full_items_db, f, ensure_ascii=False, indent=4)
        
    print("Сохранено в scripts/final_item.json")

if __name__ == "__main__":
    run()
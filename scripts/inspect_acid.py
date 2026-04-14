import requests
import json

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    # Использую те же куки, что и в основном скрипте (сокращенно для этого теста)
    "Cookie": "ttg-theme=dark" 
}

# Ссылка на кислоту (предположительно kislota или acid)
# в списке unique items 293 ссылки.
# Попробуем найти кислоту через поиск
url_search = "https://new.ttg.club/api/v2/item/search"
r = requests.post(url_search, json={"query": "Кислота"}, headers=HEADERS)
if r.status_code == 200:
    data = r.json()
    items = data if isinstance(data, list) else data.get('items', [])
    
    print(f"Found {len(items)} items.")
    target_slug = None
    
    for it in items:
        name = it.get('name', {}).get('rus', '')
        print(f"- {name} ({it.get('url')})")
        if "Кислота" in name:
            target_slug = it.get('url')
    
    if target_slug:
        slug = target_slug.split('/')[-1]
        print(f"Inspecting: {slug}")
        
        # качаем детали
        url_det = f"https://new.ttg.club/api/v2/item/{slug}"
        rd = requests.get(url_det, headers=HEADERS)
        if rd.status_code == 200:
            full = rd.json()
            print(json.dumps(full, indent=4, ensure_ascii=False))
        else:
            print(f"Error detail: {rd.status_code}")
    else:
        print("Не найдено")
else:
    print(f"Error search: {r.status_code}")

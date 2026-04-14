import requests
import json

URL_SEARCH = "https://new.ttg.club/api/v2/spells/search"
HEADERS = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
    'Accept': 'application/json',
    'Content-Type': 'application/json'
}

try:
    # 1. Search to get slug
    payload = {
        "search": {"exact": False, "value": "Брызги кислоты"},
        "order": [{"field": "level", "direction": "asc"}, {"field": "name", "direction": "asc"}],
        "skip": 0,
        "limit": 1
    }
    resp = requests.post(URL_SEARCH, headers=HEADERS, json=payload)
    print(f"Search Status: {resp.status_code}")
    
    slug = ""
    if resp.status_code == 200:
        data = resp.json()
        if data and len(data) > 0:
            slug = data[0]['url']
            print(f"Found slug: {slug}")
    
    if slug:
        # 2. Get Details
        URL_DETAILS = f"https://new.ttg.club/api/v2/spells/{slug}"
        resp = requests.get(URL_DETAILS, headers=HEADERS)
        print(f"Details Status: {resp.status_code}")
        if resp.status_code == 200:
            print(json.dumps(resp.json(), ensure_ascii=False, indent=2))

except Exception as e:
    print(e)

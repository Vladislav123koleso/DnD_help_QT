from pathlib import Path
import json
import sqlite3


REPO_ROOT = Path(__file__).resolve().parents[1]
DB_PATH = REPO_ROOT / "dnd_rules.db"
BACKGROUNDS_JSON = REPO_ROOT / "backgrounds_dndsu.json"
FEATS_JSON = REPO_ROOT / "feats_dndsu.json"


def load_json(path: Path):
    if not path.exists():
        raise FileNotFoundError(f"JSON file not found: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def create_tables(cursor: sqlite3.Cursor):
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS backgrounds (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slug TEXT UNIQUE,
            name TEXT,
            source TEXT,
            description TEXT,
            skill_prof TEXT,
            tool_prof TEXT,
            languages TEXT,
            equipment TEXT,
            feature_name TEXT,
            feature_description TEXT,
            traits TEXT
        )
        """
    )

    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS feats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slug TEXT UNIQUE,
            name TEXT,
            source TEXT,
            description TEXT,
            prerequisite TEXT,
            benefits TEXT
        )
        """
    )


def import_backgrounds(cursor: sqlite3.Cursor, backgrounds):
    cursor.execute("DELETE FROM backgrounds")
    cursor.executemany(
        """
        INSERT INTO backgrounds (
            slug, name, source, description, skill_prof, tool_prof,
            languages, equipment, feature_name, feature_description, traits
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        [
            (
                entry.get("slug", ""),
                entry.get("name", ""),
                entry.get("source", ""),
                entry.get("description", ""),
                json.dumps(entry.get("skillProficiencies", []), ensure_ascii=False),
                json.dumps(entry.get("toolProficiencies", []), ensure_ascii=False),
                json.dumps(entry.get("languages", []), ensure_ascii=False),
                json.dumps(entry.get("equipment", []), ensure_ascii=False),
                entry.get("featureName", ""),
                entry.get("featureDescription", ""),
                json.dumps(entry.get("traits", {}), ensure_ascii=False),
            )
            for entry in backgrounds
        ],
    )


def import_feats(cursor: sqlite3.Cursor, feats):
    cursor.execute("DELETE FROM feats")
    cursor.executemany(
        """
        INSERT INTO feats (
            slug, name, source, description, prerequisite, benefits
        ) VALUES (?, ?, ?, ?, ?, ?)
        """,
        [
            (
                entry.get("slug", ""),
                entry.get("name", ""),
                entry.get("source", ""),
                entry.get("description", ""),
                entry.get("prerequisite", ""),
                json.dumps(entry.get("benefits", []), ensure_ascii=False),
            )
            for entry in feats
        ],
    )


def main():
    backgrounds = load_json(BACKGROUNDS_JSON)
    feats = load_json(FEATS_JSON)

    with sqlite3.connect(DB_PATH) as connection:
        cursor = connection.cursor()
        create_tables(cursor)
        import_backgrounds(cursor, backgrounds)
        import_feats(cursor, feats)
        connection.commit()

        background_count = cursor.execute("SELECT COUNT(*) FROM backgrounds").fetchone()[0]
        feat_count = cursor.execute("SELECT COUNT(*) FROM feats").fetchone()[0]

    print(f"Imported {background_count} backgrounds and {feat_count} feats into {DB_PATH}")


if __name__ == "__main__":
    main()
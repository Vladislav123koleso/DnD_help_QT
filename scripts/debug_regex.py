import re

text = "Вы запускаете частицу света в одно существо или предмет в пределах дистанции. Совершите {@i дальнобойную атаку заклинанием} против цели. При попадании цель получает 1к8 урона излучением и до конца вашего следующего хода излучает {@glossary тусклый свет|url:dim-light-phb} в радиусе 10 футов, а также не получает эффекты от  состояния {@glossary невидимый|url:invisible-phb}."

print("Original:", text)

# Improved Regex

# Step 1: Handle tags with pipe |
# Ensure we don't cross closing braces
# pattern: {@tagname content|url}
# content shouldn't contain '|' usually, but definitely shouldn't contain '}' if we aren't careful.
# best way: look for | inside the tag.
# \{@[^\s}]+ -> tag
# \s -> space
# ([^}|]+?) -> content (non-greedy, no pipe, no closing brace)
# \| -> pipe
# [^}]+ -> url/metadata
# \} -> end
text = "Вы запускаете частицу света в одно существо или предмет в пределах дистанции. Совершите {@i дальнобойную атаку заклинанием} против цели. При попадании цель получает 1к8 урона излучением и до конца вашего следующего хода излучает {@glossary тусклый свет|url:dim-light-phb} в радиусе 10 футов, а также не получает эффекты от  состояния {@glossary невидимый|url:invisible-phb}."

print("Original Redux:", text)

text = re.sub(r"\{@[^\s}]+ ([^}|]+)\|[^}]+\}", r"\1", text)
print("After Fix 1:", text)

# Step 2: Handle remaining tags
text = re.sub(r"\{@b ([^}]+)\}", r"<b>\1</b>", text)
text = re.sub(r"\{@i ([^}]+)\}", r"<i>\1</i>", text)
print("After Fix 2:", text)

# Step 3: Catch all
text = re.sub(r"\{@[^\s}]+ ([^}]+)\}", r"\1", text)
print("After Fix 3:", text)


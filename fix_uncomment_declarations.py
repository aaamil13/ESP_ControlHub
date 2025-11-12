import re

# Read the file
with open('lib/Core/ModuleManager.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Uncomment specific variable declarations that were wrongly commented
patterns = [
    r'(\s+)// (JsonObject modulesObj = doc\["modules"\]\.to<JsonObject>\(\);)',
    r'(\s+)// (JsonObject security = doc\["security"\]\.to<JsonObject>\(\);)',
    r'(\s+)// (File file = LittleFS\.open\(configPath, "w"\);)',
    r'(\s+)// (JsonObject byType = doc\["by_type"\]\.to<JsonObject>\(\);)',
    r'(\s+)// (JsonObject byState = doc\["by_state"\]\.to<JsonObject>\(\);)'
]

for pattern in patterns:
    content = re.sub(pattern, r'\1\2', content)
    print(f"Uncommented pattern: {pattern[:50]}...")

# Write back
with open('lib/Core/ModuleManager.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("\nUncommented wrongly commented declarations")

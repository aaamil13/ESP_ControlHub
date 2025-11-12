import re

# Read the file
with open('lib/PlcEngine/Engine/PlcProgram.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace std::make_unique<Type>(args) with std::unique_ptr<PlcBlock>(new Type(args))
# This pattern handles both empty and non-empty argument lists
pattern = r'std::make_unique<([^>]+)>\(([^)]*)\)'
replacement = r'std::unique_ptr<PlcBlock>(new \1(\2))'

content = re.sub(pattern, replacement, content)

# Write back
with open('lib/PlcEngine/Engine/PlcProgram.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Replaced all std::make_unique with std::unique_ptr + new")

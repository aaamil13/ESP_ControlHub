import re

# Read the file
with open('lib/Core/ModuleManager.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Process the file line by line
i = 0
while i < len(lines):
    line = lines[i]

    # Check if this is a non-commented line that's a continuation
    # (starts with spaces and has content, but no //)
    if line.strip() and not line.strip().startswith('//'):
        # Check if previous line is a commented continuation line
        if i > 0:
            prev_line = lines[i - 1]
            # If previous line is commented and doesn't end with semicolon or {
            if '//' in prev_line and not prev_line.rstrip().endswith((';', '{', '}')):
                # Check if this line looks like a continuation (starts with String or other expression)
                stripped = line.lstrip()
                # If it doesn't start with keywords that indicate new statement
                if not any(stripped.startswith(kw) for kw in ['if', 'for', 'while', 'return', 'bool', 'void', 'int', 'String ', 'Module', 'const', 'auto', 'std::', '//', 'case', 'default', 'break']):
                    # This is likely a continuation line that needs commenting
                    # But check if it's actually C++ code by looking for common patterns
                    if ('String(' in line or '+ ' in line or ');' in line or 'String' in line):
                        # Add // prefix
                        indent = line[:len(line) - len(stripped)]
                        lines[i] = indent + '// ' + stripped
                        print(f"Fixed line {i+1}: {line.strip()[:50]}")

    i += 1

# Write the fixed file
with open('lib/Core/ModuleManager.cpp', 'w', encoding='utf-8') as f:
    f.writelines(lines)

print("\nFixed remaining multi-line comments in ModuleManager.cpp")

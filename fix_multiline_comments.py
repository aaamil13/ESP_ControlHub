import re

# Read the file
with open('lib/Core/ModuleManager.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Process the file
i = 0
while i < len(lines):
    line = lines[i]

    # Check if this is a commented logger->log line
    if '// logger->log' in line or '// EspHubLog->log' in line:
        # This is the start of a multi-line commented statement
        # Find the continuation line
        if i + 1 < len(lines):
            next_line = lines[i + 1]
            # If next line is not commented and has content, it's the continuation
            if not next_line.strip().startswith('//') and next_line.strip():
                # Add // prefix to the continuation line
                # Preserve indentation
                stripped = next_line.lstrip()
                indent = next_line[:len(next_line) - len(stripped)]
                lines[i + 1] = indent + '// ' + stripped

    i += 1

# Write the fixed file
with open('lib/Core/ModuleManager.cpp', 'w', encoding='utf-8') as f:
    f.writelines(lines)

print("Fixed multi-line comments in ModuleManager.cpp")

#!/usr/bin/env python3
"""Fix ALL remaining logging API calls"""

import re

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    new_lines = []
    for line in lines:
        # Check if this line contains ->log(LogLevel
        if '->log(LogLevel' in line:
            # Simply replace the entire log() call with println()
            # Extract just the message part

            # Handle multiline log calls
            if line.strip().endswith(');'):
                # Single line - extract message
                match = re.search(r'->log\(LogLevel::\w+, "[^"]+", (.+)\);', line)
                if match:
                    message = match.group(1)
                    indent = line[:len(line) - len(line.lstrip())]
                    logger_var = 'logger' if 'logger->' in line else 'EspHubLog'
                    new_line = f'{indent}{logger_var}->println({message});\n'
                    new_lines.append(new_line)
                    continue
            else:
                # Multiline - just comment it out for now
                new_lines.append('    // FIXME: ' + line)
                continue

        new_lines.append(line)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

# Fix both files
files = [
    r'd:\Dev\ESP\EspHub\lib\Core\ModuleManager.cpp',
    r'd:\Dev\ESP\EspHub\lib\Protocols\Mqtt\MqttManager.cpp'
]

for filepath in files:
    try:
        fix_file(filepath)
        print(f'Fixed: {filepath}')
    except Exception as e:
        print(f'Error: {filepath}: {e}')

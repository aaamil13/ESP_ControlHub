#!/usr/bin/env python3
"""Fix logging API calls to use println/printf instead of non-existent log() method"""

import re

def fix_logging(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Remove all LogLevel:: references and log() calls
    # Replace: logger->log(LogLevel::INFO, "Component", "message");
    # With: logger->println("[Component] message");

    # Pattern 1: log with concatenated strings
    content = re.sub(
        r'logger->log\(LogLevel::\w+, "([^"]+)", String\("([^"]+)"\) \+ ([^;]+)\);',
        r'logger->println(String("[ModuleManager] \2") + \3);',
        content
    )

    # Pattern 2: log with string variable
    content = re.sub(
        r'logger->log\(LogLevel::\w+, "([^"]+)", ([^;]+)\);',
        r'logger->println(\2);',
        content
    )

    # Pattern 3: EspHubLog->log calls in MqttManager
    content = re.sub(
        r'EspHubLog->log\(LogLevel::\w+, "([^"]+)", String\("([^"]+)"\) \+ ([^;]+)\);',
        r'EspHubLog->println(String("[MqttManager] \2") + \3);',
        content
    )

    content = re.sub(
        r'EspHubLog->log\(LogLevel::\w+, "([^"]+)", ([^;]+)\);',
        r'EspHubLog->println(\2);',
        content
    )

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

# Fix both files
files = [
    r'd:\Dev\ESP\EspHub\lib\Core\ModuleManager.cpp',
    r'd:\Dev\ESP\EspHub\lib\Protocols\Mqtt\MqttManager.cpp'
]

for filepath in files:
    if fix_logging(filepath):
        print(f'Fixed: {filepath}')
    else:
        print(f'No changes: {filepath}')

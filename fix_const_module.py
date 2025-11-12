import re

# Read the file
with open('lib/Core/ModuleManager.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# List of const methods where Module* should be const Module*
const_methods = [
    'checkDependencies',
    'getDependentModules',
    'getMissingDependencies',
    'getModuleInfo',
    'getModuleStatistics',
    'isModuleHealthy'
]

# For each const method, find it and replace Module* with const Module*
# But only within that method's scope
for method in const_methods:
    # Find the method definition
    pattern = rf'(bool|std::vector<String>|JsonDocument)\s+ModuleManager::{method}\s*\([^)]*\)\s*const\s*\{{'

    match = re.search(pattern, content)
    if match:
        # Find the end of this method (next method or class end)
        start_pos = match.start()

        # Find matching closing brace for this method
        brace_count = 0
        in_method = False
        end_pos = start_pos

        for i in range(start_pos, len(content)):
            if content[i] == '{':
                brace_count += 1
                in_method = True
            elif content[i] == '}':
                brace_count -= 1
                if in_method and brace_count == 0:
                    end_pos = i + 1
                    break

        # Extract the method text
        method_text = content[start_pos:end_pos]

        # Replace Module* with const Module* (but not const Module* already)
        # Look for patterns like "Module* module = getModule"
        modified_method = re.sub(
            r'(?<!const\s)Module\*(\s+\w+\s*=\s*getModule)',
            r'const Module*\1',
            method_text
        )

        # Replace in the original content
        content = content[:start_pos] + modified_method + content[end_pos:]

# Write back
with open('lib/Core/ModuleManager.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print(f"Fixed const Module* in {len(const_methods)} methods")

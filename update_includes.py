#!/usr/bin/env python3
"""
Script to update all #include paths after code restructuring.
This script updates include statements to match the new directory structure.
"""

import os
import re
from pathlib import Path

# Define include path mappings (old -> new)
INCLUDE_MAPPINGS = {
    # Core files
    '"EspHubLib.h"': '"EspHub.h"',  # When in same dir or for external files

    # Protocol files
    '"MqttManager.h"': '"../Protocols/Mqtt/MqttManager.h"',
    '"MeshDeviceManager.h"': '"../Protocols/Mesh/MeshDeviceManager.h"',
    '"mesh_protocol.h"': '"../Protocols/Mesh/mesh_protocol.h"',
    '"WiFiDeviceManager.h"': '"../Protocols/WiFi/WiFiDeviceManager.h"',
    '"RF433Manager.h"': '"../Protocols/RF433/RF433Manager.h"',
    '"ZigbeeManager.h"': '"../Protocols/Zigbee/ZigbeeManager.h"',
    '"ProtocolManagerInterface.h"': '"../Protocols/ProtocolManagerInterface.h"',

    # PLC Engine files
    '"../PlcCore/PlcEngine.h"': '"../PlcEngine/Engine/PlcEngine.h"',
    '"../PlcCore/PlcMemory.h"': '"../PlcEngine/Engine/PlcMemory.h"',
    '"../PlcCore/PlcProgram.h"': '"../PlcEngine/Engine/PlcProgram.h"',
    '"PlcEngine.h"': '"../PlcEngine/Engine/PlcEngine.h"',
    '"PlcMemory.h"': '"../PlcEngine/Engine/PlcMemory.h"',
    '"PlcProgram.h"': '"../PlcEngine/Engine/PlcProgram.h"',
    '"../PlcCore/blocks/': '"../PlcEngine/Blocks/',
    '"blocks/': '"../Blocks/',  # For files within PlcEngine

    # Export system files
    '"VariableRegistry.h"': '"../Export/VariableRegistry.h"',
    '"MqttExportManager.h"': '"../Export/MqttExportManager.h"',
    '"MeshExportManager.h"': '"../Export/MeshExportManager.h"',
    '"MqttDiscoveryManager.h"': '"../Export/MqttDiscoveryManager.h"',

    # Device management files
    '"DeviceConfigManager.h"': '"../Devices/DeviceConfigManager.h"',
    '"DeviceManager.h"': '"../Devices/DeviceManager.h"',
    '"DeviceRegistry.h"': '"../Devices/DeviceRegistry.h"',

    # Storage files
    '"UserManager.h"': '"../Storage/UserManager.h"',
    '"OtaManager.h"': '"../Storage/OtaManager.h"',

    # UI files
    '"WebManager.h"': '"../UI/WebManager.h"',

    # Apps files
    '"AppManager.h"': '"../Apps/AppManager.h"',

    # Core files (when referenced from other modules)
    '"StreamLogger.h"': '"../Core/StreamLogger.h"',
    '"Logger.h"': '"../Core/Logger.h"',
    '"TimeManager.h"': '"../Core/TimeManager.h"',
}

# Context-specific mappings (only apply in specific directories)
CONTEXT_MAPPINGS = {
    'lib/Core': {
        # When in Core, some includes are local
        '"../Core/StreamLogger.h"': '"StreamLogger.h"',
        '"../Core/Logger.h"': '"Logger.h"',
        '"../Core/TimeManager.h"': '"TimeManager.h"',
    },
    'lib/PlcEngine/Engine': {
        # When in PlcEngine/Engine, these are local
        '"../PlcEngine/Engine/PlcEngine.h"': '"PlcEngine.h"',
        '"../PlcEngine/Engine/PlcMemory.h"': '"PlcMemory.h"',
        '"../PlcEngine/Engine/PlcProgram.h"': '"PlcProgram.h"',
    },
    'lib/Protocols/Mqtt': {
        '"../Protocols/Mqtt/MqttManager.h"': '"MqttManager.h"',
    },
    'lib/Protocols/Mesh': {
        '"../Protocols/Mesh/MeshDeviceManager.h"': '"MeshDeviceManager.h"',
        '"../Protocols/Mesh/mesh_protocol.h"': '"mesh_protocol.h"',
    },
    'lib/Protocols/WiFi': {
        '"../Protocols/WiFi/WiFiDeviceManager.h"': '"WiFiDeviceManager.h"',
    },
    'lib/Protocols/RF433': {
        '"../Protocols/RF433/RF433Manager.h"': '"RF433Manager.h"',
    },
    'lib/Protocols/Zigbee': {
        '"../Protocols/Zigbee/ZigbeeManager.h"': '"ZigbeeManager.h"',
    },
    'lib/Export': {
        '"../Export/VariableRegistry.h"': '"VariableRegistry.h"',
        '"../Export/MqttExportManager.h"': '"MqttExportManager.h"',
        '"../Export/MeshExportManager.h"': '"MeshExportManager.h"',
        '"../Export/MqttDiscoveryManager.h"': '"MqttDiscoveryManager.h"',
    },
    'lib/Devices': {
        '"../Devices/DeviceConfigManager.h"': '"DeviceConfigManager.h"',
        '"../Devices/DeviceManager.h"': '"DeviceManager.h"',
        '"../Devices/DeviceRegistry.h"': '"DeviceRegistry.h"',
    },
    'lib/Storage': {
        '"../Storage/UserManager.h"': '"UserManager.h"',
        '"../Storage/OtaManager.h"': '"OtaManager.h"',
    },
    'lib/UI': {
        '"../UI/WebManager.h"': '"WebManager.h"',
    },
    'lib/Apps': {
        '"../Apps/AppManager.h"': '"AppManager.h"',
    },
}

def get_relative_path(file_path, root_dir):
    """Get relative path from root directory."""
    return os.path.relpath(file_path, root_dir).replace('\\', '/')

def get_context_dir(file_path, root_dir):
    """Get the context directory for context-specific mappings."""
    rel_path = get_relative_path(file_path, root_dir)
    for context_dir in CONTEXT_MAPPINGS.keys():
        if rel_path.startswith(context_dir):
            return context_dir
    return None

def update_includes_in_file(file_path, root_dir):
    """Update include statements in a single file."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content
        context_dir = get_context_dir(file_path, root_dir)

        # Apply context-specific mappings first (higher priority)
        if context_dir and context_dir in CONTEXT_MAPPINGS:
            for old_include, new_include in CONTEXT_MAPPINGS[context_dir].items():
                pattern = r'#include\s+' + re.escape(old_include)
                replacement = '#include ' + new_include
                content = re.sub(pattern, replacement, content)

        # Apply global mappings
        for old_include, new_include in INCLUDE_MAPPINGS.items():
            pattern = r'#include\s+' + re.escape(old_include)
            replacement = '#include ' + new_include
            content = re.sub(pattern, replacement, content)

        # Write back if changed
        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True

        return False

    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False

def main():
    """Main function to update all files."""
    root_dir = Path(__file__).parent
    lib_dir = root_dir / 'lib'
    src_dir = root_dir / 'src'

    updated_files = []
    skipped_files = []

    # Process all .h and .cpp files in lib/
    for file_path in lib_dir.rglob('*.h'):
        if update_includes_in_file(file_path, root_dir):
            updated_files.append(str(file_path))
        else:
            skipped_files.append(str(file_path))

    for file_path in lib_dir.rglob('*.cpp'):
        if update_includes_in_file(file_path, root_dir):
            updated_files.append(str(file_path))
        else:
            skipped_files.append(str(file_path))

    # Process src/ directory if it exists
    if src_dir.exists():
        for file_path in src_dir.rglob('*.h'):
            if update_includes_in_file(file_path, root_dir):
                updated_files.append(str(file_path))
            else:
                skipped_files.append(str(file_path))

        for file_path in src_dir.rglob('*.cpp'):
            if update_includes_in_file(file_path, root_dir):
                updated_files.append(str(file_path))
            else:
                skipped_files.append(str(file_path))

        for file_path in src_dir.rglob('*.ino'):
            if update_includes_in_file(file_path, root_dir):
                updated_files.append(str(file_path))
            else:
                skipped_files.append(str(file_path))

    # Print summary
    print(f"\n{'='*60}")
    print(f"Include Path Update Summary")
    print(f"{'='*60}")
    print(f"Updated files: {len(updated_files)}")
    print(f"Skipped files: {len(skipped_files)}")
    print(f"{'='*60}\n")

    if updated_files:
        print("Updated files:")
        for file_path in updated_files:
            print(f"  - {file_path}")

    print(f"\nDone!")

if __name__ == '__main__':
    main()

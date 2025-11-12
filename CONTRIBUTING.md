# Contributing to EspHub

Thank you for your interest in contributing to EspHub! 🎉

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Pull Request Process](#pull-request-process)
- [Issue Guidelines](#issue-guidelines)
- [Documentation](#documentation)

## 🤝 Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to uphold this code:

- **Be respectful** of differing viewpoints and experiences
- **Accept constructive criticism** gracefully
- **Focus on what is best** for the community
- **Show empathy** towards other community members

## 🚀 Getting Started

### Prerequisites

- **Git** - Version control
- **PlatformIO** - Build system (recommended)
- **ESP32 hardware** - For testing
- **Basic knowledge** of C++, ESP32, and IoT protocols

### Areas to Contribute

We welcome contributions in:

- 🐛 **Bug fixes** - Fix issues reported on GitHub
- ✨ **New features** - Implement features from roadmap
- 📝 **Documentation** - Improve guides and examples
- 🧪 **Testing** - Add unit/integration tests
- 🎨 **Web UI** - Improve web interface
- 🌐 **Translations** - Bulgarian/English documentation

## 💻 Development Setup

### 1. Fork and Clone

```bash
# Fork the repository on GitHub
git clone https://github.com/YOUR_USERNAME/esphub.git
cd esphub
```

### 2. Install Dependencies

```bash
# Install PlatformIO
pip install platformio

# Install library dependencies
platformio lib install
```

### 3. Build Project

```bash
# Build for ESP32
platformio run -e esp32_full

# Upload to device
platformio run -e esp32_full --target upload

# Monitor serial output
platformio device monitor -b 115200
```

### 4. Create Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/bug-description
```

## 📏 Coding Standards

### C++ Style Guide

Follow these conventions for consistency:

#### File Naming
- **Headers**: `CamelCase.h` (e.g., `ZoneManager.h`)
- **Implementation**: `CamelCase.cpp` (e.g., `ZoneManager.cpp`)

#### Code Style

```cpp
// Class names: CamelCase
class ZoneManager {
public:
    // Method names: camelCase
    void begin();
    bool addRoute(const String& zoneName);

    // Getters: camelCase with "get" prefix
    size_t getRouteCount() const;

private:
    // Private members: camelCase with underscore prefix (optional)
    ZoneRouter* _router;

    // Or no prefix (also acceptable)
    ZoneRouter* router;
};

// Constants: UPPER_CASE
#define MAX_DEVICES_PER_ZONE 30
const int DEFAULT_TIMEOUT = 5000;

// Enums: CamelCase for type, UPPER_CASE for values
enum class ZoneRole {
    UNASSIGNED,
    MEMBER,
    COORDINATOR
};
```

#### Comments

```cpp
// Single-line comments for brief explanations
int count = 0; // Track device count

/**
 * Multi-line comments for function/class documentation
 * @param zoneName - Name of zone to join
 * @param deviceName - Name of this device
 * @return true if successful, false otherwise
 */
bool joinZone(const String& zoneName, const String& deviceName);
```

#### Memory Management

```cpp
// Prefer stack allocation
ZoneManager manager;

// Use smart pointers for dynamic allocation (if needed)
std::unique_ptr<ZoneRouter> router(new ZoneRouter());

// Always delete manually allocated memory
ZoneRouter* router = new ZoneRouter();
delete router; // Don't forget!

// Check for nullptr
if (router != nullptr) {
    router->begin();
}
```

### Documentation Standards

#### Code Documentation

Use Doxygen-style comments:

```cpp
/**
 * @brief Subscribe to remote endpoint
 *
 * Subscribes to an endpoint in same or different zone.
 * For local subscriptions, adds to coordinator's registry.
 * For remote subscriptions, routes through coordinators.
 *
 * @param endpoint Full endpoint path (e.g., "kitchen.temp.value.real")
 * @param subscriberName This device's name
 * @return true if subscription successful, false otherwise
 *
 * @note Requires coordinator to be elected in target zone
 * @see unsubscribeFromEndpoint()
 */
bool subscribeToEndpoint(const String& endpoint, const String& subscriberName);
```

#### Markdown Documentation

Use clear structure:

```markdown
# Component Name

Brief description (1-2 sentences).

## Features

- Feature 1
- Feature 2

## Usage

### Basic Example

\`\`\`cpp
// Code example with comments
ZoneManager manager;
manager.begin("device", "zone");
\`\`\`

## API Reference

| Method | Description | Returns |
|--------|-------------|---------|
| `begin()` | Initialize manager | void |
```

## 🔀 Pull Request Process

### Before Submitting

1. **Test your changes** - Verify on real hardware
2. **Update documentation** - Add/update relevant docs
3. **Follow coding standards** - Match existing style
4. **Add examples** - If adding new feature
5. **Update CHANGELOG.md** - Document changes

### PR Checklist

- [ ] Code compiles without errors
- [ ] No new warnings introduced
- [ ] Tested on ESP32 hardware
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] Commit messages are clear
- [ ] PR description explains changes

### Commit Messages

Use clear, descriptive commit messages:

```bash
# Good commits
git commit -m "feat(zone-mesh): add route discovery timeout"
git commit -m "fix(plc): correct output ownership validation"
git commit -m "docs(zone-mesh): add troubleshooting section"

# Commit message format
<type>(<scope>): <subject>

# Types:
# feat     - New feature
# fix      - Bug fix
# docs     - Documentation
# style    - Formatting, no code change
# refactor - Code restructuring
# test     - Adding tests
# chore    - Maintenance
```

### PR Description Template

```markdown
## Description
Brief description of changes.

## Motivation
Why is this change needed?

## Changes
- Change 1
- Change 2

## Testing
How was this tested?

## Checklist
- [ ] Tested on hardware
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
```

### Review Process

1. **Automated checks** - CI/CD pipeline runs
2. **Code review** - Maintainer reviews code
3. **Feedback** - Address review comments
4. **Approval** - Maintainer approves PR
5. **Merge** - PR is merged to main

## 🐛 Issue Guidelines

### Reporting Bugs

Use the bug report template:

```markdown
**Describe the bug**
Clear description of the bug.

**To Reproduce**
Steps to reproduce:
1. Go to '...'
2. Click on '...'
3. See error

**Expected behavior**
What you expected to happen.

**Hardware**
- Board: ESP32-DevKit
- Flash: 4MB
- RAM: 320KB

**Software**
- EspHub version: v1.0.0
- PlatformIO version: 6.1.0
- Arduino Core: 2.0.14

**Logs**
```
Paste relevant logs here
```

**Additional context**
Any other relevant information.
```

### Feature Requests

Use the feature request template:

```markdown
**Is your feature request related to a problem?**
Clear description of the problem.

**Describe the solution you'd like**
How should this feature work?

**Describe alternatives you've considered**
Other approaches you've thought about.

**Additional context**
Mockups, diagrams, examples, etc.
```

## 📚 Documentation

### Documentation Structure

- **README.md** - Project overview and quick start
- **docs/ZoneMesh_Guide.md** - Zone mesh architecture and API
- **docs/IOEventManager_Guide.md** - Event-driven system guide
- **CHANGELOG.md** - Version history and changes
- **CONTRIBUTING.md** - This file

### Adding Documentation

When adding a new feature:

1. **Update README.md** - Add to feature list
2. **Create dedicated guide** - If major feature (e.g., `docs/NewFeature_Guide.md`)
3. **Add examples** - Provide usage examples
4. **Update API reference** - Document public methods
5. **Add to CHANGELOG.md** - Record changes

### Documentation Style

- **Use Bulgarian or English** - Both are acceptable
- **Be concise** - Clear and to the point
- **Include examples** - Code snippets for clarity
- **Use diagrams** - ASCII art or Mermaid diagrams
- **Link related docs** - Cross-reference where relevant

## 🧪 Testing

### Manual Testing

Before submitting PR:

1. **Compile test** - Ensure code compiles
2. **Hardware test** - Test on ESP32 device
3. **Integration test** - Test with other components
4. **Edge cases** - Test boundary conditions

### Unit Tests (Future)

```cpp
// Example unit test structure (TODO)
#include <unity.h>

void test_zone_manager_init() {
    ZoneManager manager;
    manager.begin("test.device", "test");
    TEST_ASSERT_EQUAL_STRING("test", manager.getZoneName().c_str());
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_zone_manager_init);
    UNITY_END();
}
```

## 💡 Tips for Contributors

### Good First Issues

Look for issues labeled:
- `good first issue` - Easy for newcomers
- `help wanted` - Maintainers need help
- `documentation` - Documentation improvements

### Communication

- **Ask questions** - Don't hesitate to ask for clarification
- **Discuss before implementing** - For major changes, discuss in issue first
- **Be patient** - Reviews may take a few days
- **Be open to feedback** - Embrace constructive criticism

### Resources

- **ESP32 Documentation**: https://docs.espressif.com/
- **PlatformIO Docs**: https://docs.platformio.org/
- **ArduinoJson Guide**: https://arduinojson.org/
- **EspHub Docs**: [docs/](docs/)

## 🎉 Recognition

Contributors are recognized in:
- **CHANGELOG.md** - For significant contributions
- **README.md** - In acknowledgments section
- **GitHub Contributors** - Automatic recognition

## 📞 Questions?

- **GitHub Discussions**: Ask questions and discuss ideas
- **GitHub Issues**: Report bugs or request features
- **Email**: support@esphub.io

---

Thank you for contributing to EspHub! Your efforts help make this project better for everyone. 🚀

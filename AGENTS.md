# AGENTS.md - Zephyr RTOS Project

## Overview
Welcome to the Zephyr RTOS project! This file provides guidance for AI agents working with the Zephyr codebase. Zephyr is a scalable real-time operating system (RTOS) supporting multiple hardware architectures, optimized for resource-constrained and embedded devices.

## Code Style and Conventions

### C/C++ Code Style
- Follow the [Zephyr Coding Style Guide](doc/contribute/style/code.rst)
- Be very mindful of Coding guidelines as well (MISRA -- doc/contribute/coding_guidelines/index.rst))
- Use Linux kernel coding style as the base
- Indentation: Use tabs, not spaces (tab width = 8)
- Line length: Maximum 100 characters (exceptions for function signatures)
- Function naming: Use lowercase with underscores (`my_function_name`)
- Macro naming: Use uppercase with underscores (`MY_MACRO_NAME`)
- Variable naming: Use lowercase with underscores
- Struct/enum naming: Use lowercase with underscores

### Device Tree
- Follow Device Tree conventions and Zephyr's bindings
- Use appropriate vendor prefixes
- Keep node names concise and descriptive
- Always validate device tree changes with appropriate tools

### Kconfig
- Follow existing Kconfig patterns
- Use appropriate help text that explains the option clearly
- Consider dependencies and conflicts with other options
- Test with different configuration combinations

## Project Structure

### Key Directories
- `arch/` - Architecture-specific code
- `boards/` - Board support packages (BSPs)
- `drivers/` - Device drivers
- `dts/` - Device tree source files and bindings
- `include/` - Public headers
- `kernel/` - Core kernel implementation
- `lib/` - Libraries and utilities
- `modules/` - External modules integration
- `samples/` - Example applications
- `subsys/` - Subsystem implementations
- `tests/` - Test suites

### File Organization
- Keep related functionality together
- Use appropriate directory structure for new features
- Follow existing patterns for similar components
- Separate public APIs from internal implementation

## Development Practices

### Documentation
- Use Doxygen comments for public APIs
- Include brief descriptions and parameter documentation
- Add usage examples where appropriate
- Update relevant documentation in `doc/` when adding features

### Testing
- Add appropriate tests for new functionality
- Use existing test frameworks (ztest, QEMU, etc.)
- Ensure tests cover edge cases and error conditions
- Verify tests pass on relevant platforms

### Hardware Abstraction
- Use Zephyr's HAL (Hardware Abstraction Layer) patterns
- Implement platform-agnostic solutions when possible
- Consider power management implications
- Follow interrupt handling best practices

## Pull Request Guidelines

### PR Description Template
When creating PRs, include:
- **Brief description**: What does this change do?
- **Type of change**: Bug fix, new feature, documentation, etc.
- **Testing done**: How was this tested?
- **Platforms tested**: Which boards/architectures were verified?
- **Documentation impact**: Does this require doc updates?

### Commit Messages
- Use conventional commit format when appropriate
- First line: Brief summary (50 chars or less)
- Blank line, then detailed description if needed
- Reference GitHub issues with "Fixes #123" or "Closes #123"
- Sign-off required: `Signed-off-by: Your Name <email@example.com>`

## Building and Testing

### Build System
- Use CMake-based build system
- Understand Zephyr's build process and west tool
- Test builds with different configurations
- Verify no new warnings are introduced

### Validation Commands
After making changes, run these validation steps:

```bash
# Build for common platforms
west build -p auto -b qemu_x86 samples/hello_world
west build -p auto -b qemu_cortex_m3 samples/hello_world
west build -p auto -b native_posix samples/hello_world

# Run relevant tests
west build -p auto -b qemu_x86 tests/kernel/common
west build -t run

# Check coding style (if available)
./scripts/checkpatch.pl --no-tree -f <changed_files>

# Build documentation (for doc changes)
cd doc
make html
```

### Static Analysis
- Run static analysis tools when available
- Address any new warnings or errors
- Consider using tools like sparse, cppcheck, or clang-static-analyzer

## Embedded/IoT Considerations

### Resource Constraints
- Always consider memory (RAM/ROM) usage
- Optimize for power consumption where relevant
- Consider real-time requirements and deterministic behavior
- Think about stack usage and avoid deep recursion

### Hardware Diversity
- Test on multiple architectures when possible
- Consider endianness issues
- Account for different peripheral availability
- Respect hardware-specific constraints

### Security
- Follow secure coding practices
- Consider attack vectors in IoT deployments
- Use appropriate cryptographic functions
- Validate all inputs and boundary conditions

## Common Patterns

### Error Handling
- Use appropriate return codes (negative errno values)
- Log errors at appropriate levels
- Clean up resources on error paths
- Provide meaningful error messages

### Logging
- Use Zephyr's logging subsystem
- Choose appropriate log levels
- Include relevant context in log messages
- Consider log overhead in resource-constrained environments

### Memory Management
- Prefer stack allocation when possible
- Use appropriate memory allocation APIs
- Always check allocation success
- Free resources in reverse order of allocation

## Getting Help

### Resources
- [Zephyr Documentation](https://docs.zephyrproject.org/)
- [Developer Mailing List](https://lists.zephyrproject.org/g/devel)
- [Discord Server](https://chat.zephyrproject.org/)
- [GitHub Discussions](https://github.com/zephyrproject-rtos/zephyr/discussions)

### Code Review Process
- Be responsive to review feedback
- Explain design decisions when asked
- Test suggested changes thoroughly
- Maintain professional and collaborative tone

---

**Note**: This guidance should be followed for all changes within the Zephyr project tree. When in doubt, look at existing similar code for patterns and ask the community for guidance.
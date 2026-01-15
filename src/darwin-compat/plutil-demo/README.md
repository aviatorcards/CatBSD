# plutil-demo - Property List Utility for CatBSD

Simplified property list utility demonstrating plist parsing without full CoreFoundation.

## About

`plutil` is macOS's property list utility for validating and converting .plist files. This demo version shows:

- XML plist parsing using libxml2
- Plist validation
- Human-readable output
- Foundation for launchd configuration

## Building

```bash
make
```

## Usage

### Validate a plist

```bash
./plutil-demo -lint test.plist
```

### Print plist contents

```bash
./plutil-demo -p test.plist
```

### Run tests

```bash
make test
```

## Example Output

```
$ ./plutil-demo -lint test.plist
✓ test.plist: OK

$ ./plutil-demo -p test.plist
=== Property List: test.plist ===

Dictionary:
  Key: Label
  String: com.catbsd.test
  Key: ProgramArguments
  Array:
    String: /usr/bin/test
  Key: RunAtLoad
  Boolean: true
```

## Integration with CatBSD

This utility is essential for launchd, which uses .plist files for daemon configuration.

**Full CoreFoundation port would add:**

- Binary plist support
- Plist conversion (XML ↔ binary)
- More data types
- Better error handling

---

**Status**: Working demo, ready for launchd integration

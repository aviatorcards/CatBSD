# pbcopy / pbpaste - Clipboard Utilities for CatBSD

Darwin-style clipboard utilities for command-line clipboard access.

## About

`pbcopy` and `pbpaste` are essential macOS utilities for clipboard manipulation from the command line. These demos provide Darwin-compatible interfaces.

---

## pbcopy - Copy to Clipboard

Copy data from stdin to the system clipboard.

### Usage

```bash
pbcopy-demo < file.txt           # Copy file contents
echo "text" | pbcopy-demo        # Copy text
cat data.json | pbcopy-demo      # Copy command output
```

### Examples

```bash
# Copy a file
pbcopy-demo < README.md

# Copy command output
ls -la | pbcopy-demo

# Copy text
echo "Hello, CatBSD!" | pbcopy-demo

# Copy code
cat script.sh | pbcopy-demo
```

---

## pbpaste - Paste from Clipboard

Paste data from the system clipboard to stdout.

### Usage

```bash
pbpaste-demo                     # Paste to stdout
pbpaste-demo > file.txt          # Paste to file
pbpaste-demo | grep pattern      # Pipe clipboard data
```

### Examples

```bash
# Paste to terminal
pbpaste-demo

# Save clipboard to file
pbpaste-demo > saved.txt

# Search clipboard
pbpaste-demo | grep "error"

# Process clipboard data
pbpaste-demo | wc -l
```

---

## Combined Workflows

### Copy and Paste

```bash
# Copy
echo "CatBSD rocks!" | pbcopy-demo

# Paste
pbpaste-demo
# Output: CatBSD rocks!
```

### File Processing

```bash
# Copy file, modify, save
cat input.txt | pbcopy-demo
pbpaste-demo | sed 's/old/new/g' > output.txt
```

### Code Snippets

```bash
# Copy function from code
grep -A 10 "function_name" code.c | pbcopy-demo

# Paste and compile
pbpaste-demo > snippet.c
cc snippet.c -o snippet
```

### Data Transfer

```bash
# On one terminal
cat data.json | pbcopy-demo

# On another terminal
pbpaste-demo | jq '.'
```

---

## Building

```bash
# Build pbcopy
cd pbcopy-demo
make

# Build pbpaste
cd pbpaste-demo
make
```

---

## Testing

```bash
# Test pbcopy
cd pbcopy-demo
make test

# Test pbpaste
cd pbpaste-demo
make test

# Manual test
echo "Test" | ./pbcopy-demo
./pbpaste-demo
```

---

## Implementation Notes

**Current**: Wraps macOS `pbcopy`/`pbpaste` commands  
**Future**: Direct clipboard API integration

### Features

- ✅ Stdin/stdout handling
- ✅ Large data support (10MB limit)
- ✅ Error handling
- ✅ Darwin-compatible interface

### Limitations

- Requires macOS clipboard (for now)
- Text-only (no rich content)
- No clipboard history

---

## Integration with CatBSD

These utilities are essential for:

- Developer workflows
- Shell scripting
- Data transfer between commands
- Terminal productivity

---

**Status**: Working! Copy and paste from command line! 📋

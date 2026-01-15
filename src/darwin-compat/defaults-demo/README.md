# defaults - User Defaults System for CatBSD

Darwin-style user preference management system.

## About

`defaults` is macOS's command-line interface to the user defaults system. This demo provides Darwin-compatible preference storage using property lists.

## Usage

### Write a Preference

```bash
defaults-demo write <domain> <key> <value>
```

### Read a Preference

```bash
defaults-demo read <domain> <key>
```

### Delete a Preference

```bash
defaults-demo delete <domain> <key>
```

### List All Domains

```bash
defaults-demo domains
```

---

## Examples

### Application Preferences

```bash
# Set app theme
defaults-demo write com.catbsd.editor theme dark

# Set font
defaults-demo write com.catbsd.editor font "Monaco 12"

# Read preferences
defaults-demo read com.catbsd.editor theme
# Output: dark

# Delete preference
defaults-demo delete com.catbsd.editor font
```

### System Settings

```bash
# Window behavior
defaults-demo write com.catbsd.windowmanager focus "click-to-focus"

# Terminal settings
defaults-demo write com.catbsd.terminal scrollback 10000

# Read settings
defaults-demo read com.catbsd.windowmanager focus
```

### Fun Examples

```bash
# Cat mode! 🐱
defaults-demo write com.catbsd.system meow "🐱"
defaults-demo write com.catbsd.system purr "enabled"

# Read cat settings
defaults-demo read com.catbsd.system meow
# Output: 🐱
```

---

## Storage Location

Preferences are stored as XML plists in:

```
~/Library/Preferences/<domain>.plist
```

Example:

```
~/Library/Preferences/com.catbsd.editor.plist
```

---

## Integration with Other Utilities

### With plutil

```bash
# Write preferences
defaults-demo write com.catbsd.app setting value

# View the plist
plutil-demo -p ~/Library/Preferences/com.catbsd.app.plist
```

### With pbcopy/pbpaste

```bash
# Copy preferences to clipboard
cat ~/Library/Preferences/com.catbsd.app.plist | pbcopy-demo

# Paste and validate
pbpaste-demo > /tmp/prefs.plist
plutil-demo -lint /tmp/prefs.plist
```

### With say

```bash
# Announce preference changes
defaults-demo write com.catbsd.app theme dark
say-demo "Theme changed to dark mode"
```

---

## Workflow Examples

### Application Configuration

```bash
# Configure text editor
defaults-demo write com.catbsd.editor theme "solarized-dark"
defaults-demo write com.catbsd.editor font "Menlo 14"
defaults-demo write com.catbsd.editor tabwidth 4
defaults-demo write com.catbsd.editor autosave true

# List all editor settings
defaults-demo domains | grep editor
```

### Backup and Restore

```bash
# Backup preferences
cp ~/Library/Preferences/com.catbsd.app.plist ~/backup.plist

# Restore preferences
cp ~/backup.plist ~/Library/Preferences/com.catbsd.app.plist
```

### Sharing Settings

```bash
# Export settings
cat ~/Library/Preferences/com.catbsd.app.plist | pbcopy-demo

# On another machine
pbpaste-demo > ~/Library/Preferences/com.catbsd.app.plist
```

---

## Building

```bash
cd defaults-demo
make
```

---

## Testing

```bash
make test
```

This will:

1. Write several preferences
2. Read them back
3. List all domains
4. Delete a preference
5. Verify deletion

---

## Implementation Notes

**Current**:

- XML plist storage
- String values only
- Simple key-value pairs

**Future Enhancements**:

- Binary plist support
- Multiple data types (int, bool, array, dict)
- Domain wildcards
- Import/export
- Synchronization

---

## Why This Matters

The defaults system is essential for:

- **Application settings** - Apps store preferences
- **System configuration** - System-wide settings
- **User customization** - Per-user preferences
- **macOS compatibility** - Apps expect this API

---

**Status**: Working! Full preference management system! ⚙️

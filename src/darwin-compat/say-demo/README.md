# say-demo - Text-to-Speech for CatBSD

Simplified text-to-speech utility demonstrating audio output without full Speech framework.

## About

`say` is macOS's text-to-speech command. This demo version shows:

- Command-line TTS interface
- Voice selection
- Darwin-style API wrapping
- Fun demonstration of CatBSD capabilities!

## Building

```bash
make
```

## Usage

### Basic speech

```bash
./say-demo "Hello from CatBSD!"
```

### With specific voice

```bash
./say-demo -v Alex "CatBSD is purring to life"
```

### List available voices

```bash
./say-demo --voice-list
```

## Example Output

```
$ ./say-demo "CatBSD is purring to life!"
=== CatBSD say Demo ===
Text-to-Speech using Darwin compatibility

🔊 Speaking: "CatBSD is purring to life!"
✓ Speech completed
```

## Fun Examples

```bash
# Welcome message
./say-demo "Welcome to CatBSD, where FreeBSD meets Darwin"

# System status
./say-demo -v Samantha "All systems operational"

# Easter egg
./say-demo -v Daniel "Meow! CatBSD is ready to pounce"
```

## Integration with CatBSD

**Current implementation**: Wraps macOS `say` command  
**Full port would add:**

- Direct Speech framework integration
- Custom voice synthesis
- Audio file output
- SSML support

---

**Status**: Working demo, maximum fun factor! 🔊

# CatBSD Git Workflow

## Branch Strategy

### Main Branches

- **`main`** - Stable releases only (tagged versions)
- **`develop`** - Active development, integration branch

### Feature Branches

- **`feature/*`** - New utilities or major features
- **`freebsd/*`** - FreeBSD integration work
- **`docs/*`** - Documentation updates

### Example Workflow

```bash
# Start new feature
git checkout develop
git checkout -b feature/new-utility

# Work on feature
git add .
git commit -m "Add new utility"

# Merge back to develop
git checkout develop
git merge feature/new-utility

# Release (when ready)
git checkout main
git merge develop
git tag v0.2.0-alpha
```

## Current Status

**v0.1.0-alpha** (tagged on main)

- Essential 15 utilities
- Compatibility shims
- Documentation

**develop branch** (active)

- Future development
- New utilities
- FreeBSD integration

## Suggested Next Branches

```bash
# FreeBSD VM testing
git checkout -b freebsd/vm-integration

# Additional utilities
git checkout -b feature/more-utilities

# Documentation improvements
git checkout -b docs/api-reference
```

## Commit Message Format

```
<type>: <subject>

<body>

<footer>
```

**Types:**

- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `test:` Tests
- `refactor:` Code refactoring
- `chore:` Maintenance

**Example:**

```
feat: add textutil utility

Implement textutil for document conversion.
Supports txt, rtf, and html formats.

Closes #123
```

# CatBSD Source Inventory

Generated: Thu Jan 15 04:38:25 EST 2026

## FreeBSD Source

- **Location**: `src/freebsd/`
- **Branch**: stable/13
- **Commit**: a7d9d1b19

## Apple OSS Source

- **Location**: `src/apple-oss/`
- **Version**: macOS 10.8.5
- **Commit**: 4e290a9

## Key Components to Port

### High Priority
- [ ] launchd (init system)
- [ ] libdispatch (Grand Central Dispatch)
- [ ] CoreFoundation
- [ ] libsystem

### Medium Priority
- [ ] XNU kernel components (VFS enhancements)
- [ ] IOKit basics
- [ ] Security framework

### Low Priority
- [ ] Additional frameworks as needed

## Notes

- FreeBSD provides the base kernel and userland
- Darwin components will be integrated into `src/darwin-compat/`
- Patches for integration will be stored in `src/patches/`

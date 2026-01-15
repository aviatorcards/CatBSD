# Porting Checklist Template

Component: [Component Name]

## Phase 1: Extraction and Analysis

- [ ] Extract source from Apple OSS
- [ ] Copy to `darwin-compat/[component]/`
- [ ] Run dependency analysis
- [ ] Review source code structure
- [ ] Identify Mach-specific code
- [ ] List required syscalls
- [ ] Document external dependencies
- [ ] Complete component analysis document

## Phase 2: Preparation

- [ ] Create component directory structure
- [ ] Set up version control branch
- [ ] Identify needed compatibility shims
- [ ] Review existing shims for reuse
- [ ] Plan shim implementations
- [ ] Document porting strategy

## Phase 3: Compatibility Layer

- [ ] Create/update Mach port shim
- [ ] Create/update syscall shims
- [ ] Create/update XPC shim (if needed)
- [ ] Create/update IOKit shim (if needed)
- [ ] Implement component-specific shims
- [ ] Test shims independently
- [ ] Document shim APIs

## Phase 4: Build System

- [ ] Create `Makefile.bsd`
- [ ] List all source files
- [ ] Configure include paths
- [ ] Add dependency libraries
- [ ] Set compiler flags
- [ ] Test initial compilation
- [ ] Fix build errors
- [ ] Verify clean build

## Phase 5: Code Adaptation

- [ ] Replace Mach calls with shims
- [ ] Adapt Darwin syscalls to FreeBSD
- [ ] Handle missing kernel features
- [ ] Port Darwin-specific APIs
- [ ] Fix compiler warnings
- [ ] Address portability issues
- [ ] Document all changes made
- [ ] Create patch files

## Phase 6: Testing

### Unit Testing

- [ ] Identify existing tests
- [ ] Port tests to FreeBSD
- [ ] Create new tests as needed
- [ ] Run unit test suite
- [ ] Fix failing tests
- [ ] Achieve acceptable test coverage

### Integration Testing

- [ ] Test with dependent components
- [ ] Verify API compatibility
- [ ] Test error handling
- [ ] Check memory management
- [ ] Validate thread safety
- [ ] Performance testing

### Functional Testing

- [ ] Test basic functionality
- [ ] Test edge cases
- [ ] Test error conditions
- [ ] Verify expected behavior
- [ ] Compare with macOS behavior
- [ ] Document any differences

## Phase 7: Integration

- [ ] Install to staging directory
- [ ] Update library paths
- [ ] Configure system integration
- [ ] Test in minimal CatBSD environment
- [ ] Test with other ported components
- [ ] Verify no regressions
- [ ] Performance validation

## Phase 8: Documentation

- [ ] Update component analysis
- [ ] Document API changes
- [ ] Create usage examples
- [ ] Write integration guide
- [ ] Update main documentation
- [ ] Add to ported components list
- [ ] Document known issues

## Phase 9: Cleanup

- [ ] Code review
- [ ] Remove debug code
- [ ] Clean up commented code
- [ ] Verify license headers
- [ ] Update copyright notices
- [ ] Final build verification
- [ ] Commit final changes

## Status Tracking

- **Started**: [Date]
- **Current Phase**: [Phase number/name]
- **Completion**: [Percentage]
- **Blockers**: [List any blockers]
- **Target Completion**: [Date]

## Notes

[Add notes, issues encountered, solutions found, etc.]

---

**Last Updated**: [Date]  
**Status**: [Not Started / In Progress / Blocked / Complete]

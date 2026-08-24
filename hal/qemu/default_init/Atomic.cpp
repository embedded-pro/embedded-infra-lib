// Libatomic shims for single-core Cortex-M.
// On ARMv7-M, the compiler's built-in __sync/__atomic intrinsics map directly
// to LDREX/STREX instructions, so no shims are required for a single-core target.
// This translation unit exists as a placeholder; add explicit __atomic_* symbol
// definitions here if a specific toolchain configuration requires them.

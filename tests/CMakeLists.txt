# Phase 0 smoke tests. Real test suites for CPU/HLE/GPU land alongside the
# subsystems they cover in later phases.

add_executable(smoke_phase0 smoke_phase0.c)
target_link_libraries(smoke_phase0 PRIVATE switch_core)

add_test(NAME smoke_phase0 COMMAND smoke_phase0)

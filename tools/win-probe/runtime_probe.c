// runtime_probe.c - probe #2 of ADR-0010 section 2: the shipped binary is a true static
// build. Two halves, run in CI:
//   1. this program: a statically linked exe must not have a compiler runtime DLL mapped
//      in its own address space. Any of the forbidden names below present -> exit 1.
//   2. objdump -p / dumpbin /DEPENDENTS: the import table lists no non-system DLL.
//
// System DLLs (kernel32, user32, gdi32, shell32, ...) are expected and never probed here;
// the rule is about compilers, not Windows itself (ADR-0010 section 2, D7b).

#include <stdio.h>
#include <windows.h>

static const char* const kForbidden[] = {
    "libstdc++-6.dll",    "libgcc_s_seh-1.dll", "libwinpthread-1.dll", "vcruntime140.dll",
    "vcruntime140_1.dll", "msvcp140.dll",       "msvcp140_1.dll",      "msvcp140_2.dll",
};

int main(void) {
  int failures = 0;
  for (size_t i = 0; i < sizeof(kForbidden) / sizeof(kForbidden[0]); ++i) {
    if (GetModuleHandleA(kForbidden[i]) != NULL) {
      printf("runtime_probe: FAIL - %s is mapped in this process\n", kForbidden[i]);
      ++failures;
    }
  }
  if (failures != 0) {
    return 1;
  }
  printf("runtime_probe: PASS - no compiler runtime DLL mapped, static link holds\n");
  return 0;
}
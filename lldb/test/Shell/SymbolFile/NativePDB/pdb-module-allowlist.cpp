// REQUIRES: lld, x86
//
// Verify that the target-wide filter runs before both PECOFF symbol vendor
// lookup and NativePDB fallback, while empty and case-insensitive allowlists
// retain symbol lookup for selected modules.
//
// RUN: rm -rf %t && mkdir -p %t
// RUN: %clang_cl --target=x86_64-windows-msvc -Od -Z7 -c /Fo%t/game.obj -- %s
// RUN: lld-link -debug:full -nodefaultlib -entry:main %t/game.obj -out:%t/game.exe -pdb:%t/game.pdb
//
// RUN: %lldb -O "settings set target.pdb-module-allowlist other.dll" \
// RUN:   -f %t/game.exe -o "image lookup -n main" -o quit | FileCheck %s --check-prefix=DENIED --implicit-check-not="Summary:"
// RUN: %lldb -O "settings set target.pdb-module-allowlist GAME.EXE" \
// RUN:   -f %t/game.exe -o "image lookup -n main" -o quit | FileCheck %s --check-prefix=ALLOWED
// RUN: %lldb -f %t/game.exe -o "image lookup -n main" -o quit | FileCheck %s --check-prefix=ALLOWED

int main() { return 0; }

// DENIED: image lookup -n main
// ALLOWED: 1 match found
// ALLOWED: Summary: {{.*}}`main

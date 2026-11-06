PackWeaponCondition
===================
An open-source, collection-specific five-tier weapon-condition adapter for Fallout 4.
GPL-3.0-only; third-party copyright and license notices are retained.

Release status
--------------
This is the first public-source candidate. There is no signed or installable public
release yet. Free SignPath signing has not been approved. The corrected native
candidate is currently blocked by Smart App Control on the development machine.
This source release does not establish that the rebuilt native code runs in game.
No change to Windows security settings is part of installation or development.

Features and requirements
-------------------------
Pristine, Worn, Damaged, Badly Damaged and Ruined tiers. Reliability and accuracy
deteriorate; base weapon damage stays unchanged. Workbench-only component repairs,
perk-limited restoration, substantial ruined rebuilds and player-only malfunctions.
Only the equipped weapon receives rain, radstorm or swimming condition loss.
The current catalog contains 703 WEAP records, including variants, and 290 AMMO
identifiers. This does not mean 703 distinct gun designs. The full configured weapon
set must be present: unresolved entries keep the adapter inactive.

Exact supported runtime: Fallout 4 1.10.163 / F4SE 0.6.23, Condition System Framework
1.1.1 and its original dependencies, including the matching IIF card ABI. The CSF
dependency SHA256 is recorded in compatibility.json and enforced by native guards.
Other runtimes/framework builds are unsupported until separately audited.

The source tree contains our adapter, own plugin-record source, generated profiles
and the vendored MinHook/HDE source. It includes no original game executable,
framework DLL/PDB, compiled SWF, upstream MCM package or third-party mod archive.
Obtain dependencies from their authors; see THIRD_PARTY_NOTICES.txt.

Build
-----
Use 64-bit Windows, Visual Studio 2022 C++ build tools and CMake 3.20 or later:
  cmake -S . -B build -A x64 -DPWC_BUILD_NATIVE_TESTS=ON
  cmake --build build --config Release
  ctest --test-dir build -C Release --output-on-failure
  python scripts/build_plugin.py
The native build configuration is prepared but has not yet run in hosted CI.
Do not use a rebuild or another host to work around an endpoint protection block.
Use the approved signing process; read docs/SigningPolicy.txt.

Source checks
-------------
  python scripts/validate_source.py
These check catalog/profile agreement, source-only publication boundaries and
reconstruction of the six-owned-record plugin. They do not execute a native DLL.
Native policy harnesses cover isolated behavior, not actual engine outcomes.
The hook harness needs independently owned image fixtures and is not run in public
CI. The source validation and native tests are separate checks.

Installation and removal
------------------------
No installable public package is available. Do not copy only individual profiles
into an existing installation. A qualified release must pair the helper, ESP and
profiles and use compatible framework settings supplied by the collection.
Keep a pre-install save/new playthrough and back up manager rules and settings.
Persistent per-stack condition requires careful rollback; do not remove components
piecemeal from a continuing save. See docs/ReleaseChecks.txt.

Code signing policy: docs/SigningPolicy.txt
Privacy: docs/Privacy.txt
License and credits: LICENSE, COPYRIGHT, THIRD_PARTY_NOTICES.txt

## Context

SatGraf is a CMake-based C++17 Qt 6 application with core, GUI, and app targets. The root CMake file currently configures Debian packaging through CPack, installs Linux desktop/icon files, and does not define macOS bundle properties or a macOS runtime dependency deployment step. The GUI target is Qt Widgets-based, and the main window already uses custom chrome, translucent background, and a right-side control panel, making it possible to add macOS-specific styling without replacing the application shell.

## Goals / Non-Goals

**Goals:**
- Produce a runnable `satgraf.app` bundle on macOS.
- Produce a distributable macOS package artifact from the current development environment, preferably a `.dmg` through CPack DragNDrop.
- Deploy Qt runtime dependencies into the bundle using Qt-supported tooling.
- Keep existing Linux configure/build/run behavior and Debian packaging behavior unchanged.
- Add a repeatable local packaging smoke test that builds, packages, inspects, and launches the macOS app/package.
- Apply macOS-only visual integration that evokes macOS 26 Liquid Glass styling through translucent surfaces, native menu/title behavior where practical, platform spacing, and readable controls.

**Non-Goals:**
- Rewriting the GUI in AppKit or SwiftUI.
- Implementing notarization, signing identity management, or App Store distribution.
- Changing graph rendering algorithms, parser behavior, solver behavior, or non-macOS UI styling.
- Requiring macOS-specific dependencies on Linux builds.

## Decisions

### D1: Keep one target and enable macOS bundle properties conditionally

Use the existing `satgraf` executable target and set `MACOSX_BUNDLE` plus bundle metadata only inside `if(APPLE)` blocks. Add bundle identifier, bundle display name, version fields, and icon resource wiring to `src/app/CMakeLists.txt`. The Linux target remains a normal executable linked against the same Qt/core/gui libraries.

**Rationale:** This preserves the current target graph and avoids a second app target that could drift from the normal executable.

**Alternative:** Add a separate `satgraf_macos` target. That would isolate macOS settings but duplicate sources, links, and install rules.

### D2: Select CPack generators by platform

Keep the existing Debian CPack configuration for Linux/non-Apple builds and add Apple-specific CPack settings with `CPACK_GENERATOR "DragNDrop"` for macOS. The macOS path installs the `.app` bundle into the package root and includes icon/license metadata suitable for a DMG.

**Rationale:** CPack already exists in the project, and DragNDrop is the simplest package format for local macOS testing and manual distribution.

**Alternative:** Use `productbuild` `.pkg`. That is useful for installer workflows but adds signing/notarization expectations and is heavier for the current request.

### D3: Use Qt deployment tooling during install/package

Prefer Qt 6 CMake deployment helpers when available; otherwise call `macdeployqt` as the fallback packaging step. The deployment step must copy Qt frameworks/plugins into `satgraf.app` so the package runs outside the build tree.

**Rationale:** Qt deployment rules are platform-specific and brittle when hand-written. Using Qt-provided tooling reduces missing plugin/framework issues.

**Alternative:** Manually copy frameworks and plugins. That is more transparent but fragile across Qt versions and Homebrew/official Qt layouts.

### D4: Generate or add a real `.icns` asset

Create an `icon.icns` from the existing icon source or add a maintained macOS icon asset, and wire it through `MACOSX_BUNDLE_ICON_FILE` and `RESOURCE` source properties.

**Rationale:** macOS bundles require an `.icns` icon for Finder, Dock, and DMG presentation. Reusing existing icon art keeps branding stable.

**Alternative:** Reuse PNG directly. macOS can load PNGs in some contexts, but `.icns` is the conventional bundle format.

### D5: Apply macOS 26 styling through Qt platform-aware branches

Add macOS-only application attributes and style rules that keep native system menus, avoid custom traffic-light replacements where Qt can provide native chrome, and use translucent/vibrant panel surfaces with conservative contrast. Non-macOS platforms retain the existing styling path.

**Rationale:** The app is Qt Widgets-based, so the pragmatic path is platform-aware styling and native integration rather than a toolkit rewrite.

**Alternative:** Use AppKit interop for NSVisualEffectView. That can improve vibrancy, but it adds Objective-C++ integration and should be reserved for a follow-up if Qt styling is not sufficient.

## Risks / Trade-offs

- **[Qt deployment differences]** → Homebrew Qt and official Qt installations may expose deployment helpers differently. **Mitigation:** Support both Qt CMake deploy helpers and `macdeployqt`, and document the detected path in the packaging command output.
- **[OpenCL dependency packaging]** → OpenCL availability varies across macOS versions and hardware. **Mitigation:** Smoke-test launch from the packaged bundle and leave runtime error reporting intact; do not bundle system OpenCL.
- **[Custom chrome conflicts]** → Existing frameless/translucent window behavior may fight macOS native window controls. **Mitigation:** Gate frame/title behavior behind `Q_OS_MACOS` and use native chrome on macOS unless testing shows a specific visual requirement needs custom chrome.
- **[Visual readability]** → Translucent surfaces can reduce contrast over graph content. **Mitigation:** Use subtle material opacity, separators, and explicit foreground colors; verify controls remain readable over a loaded graph.
- **[Linux regression]** → macOS-specific CMake or GUI code could accidentally affect Linux builds. **Mitigation:** Put macOS-only behavior behind `if(APPLE)` in CMake and `Q_OS_MACOS` in C++, and run a Linux configure/build or documented equivalent check before completing implementation.
- **[Unsigned package warnings]** → Local DMGs/apps can trigger Gatekeeper warnings when moved between machines. **Mitigation:** Keep signing/notarization out of scope but document that local smoke testing is for the current environment only.

## Migration Plan

1. Add macOS bundle metadata, icon resource wiring, install rules, and platform-specific CPack settings.
2. Add Qt runtime deployment to the macOS install/package path.
3. Add macOS-only application and main-window styling branches.
4. Add or document a local command sequence for configure, build, package, app inspection, and smoke launch.
5. Verify Linux build/run behavior and Debian packaging settings remain guarded by non-Apple conditions.

Rollback is to remove the Apple-specific CMake/style branches and the added macOS package artifact instructions; existing Linux behavior should remain unaffected throughout the change.

## Open Questions

- Should future distribution add Developer ID signing and notarization, or is local/test packaging enough for the next release?
- Should the app eventually use AppKit vibrancy through Objective-C++ if Qt stylesheet/material effects are not close enough to macOS 26?

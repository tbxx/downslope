# Packaging

Installers for the **Downslope plugin** (plugin only — no Standalone, no Host app).

| OS | Formats | Installer | How it's built |
|----|---------|-----------|----------------|
| macOS | AU + VST3 | `.pkg` | Locally on a Mac (`packaging/macos/build_pkg.sh`) |
| Windows | VST3 | `.exe` (Inno Setup) | GitHub Actions (manual trigger) |

There is no AU on Windows and no AAX (Pro Tools) in any installer.

Both installers are **unsigned** for now, so the OS shows a security warning the
user must click through (Gatekeeper on macOS, SmartScreen on Windows). See
[Adding signing later](#adding-signing-later).

---

## Windows installer (GitHub Actions)

Built on a `windows-latest` runner with MSVC. The workflow is **manual-only** —
it does not run on push — so CI minutes stay near zero.

1. Push the repo to GitHub (private is fine).
2. Repo → **Actions** tab → **Windows Build** → **Run workflow**.
3. Wait ~15–25 min. When it's green, open the run and download the
   **`Downslope-<version>-Windows-installer`** artifact from the Artifacts section.
   (Private-repo artifacts require you to be logged in to GitHub.)
4. Unzip → run `Downslope-<version>-Windows.exe`. It installs the VST3 to
   `C:\Program Files\Common Files\VST3\Downslope.vst3`.

Version is read automatically from `project(Downslope VERSION ...)` in
`CMakeLists.txt` — bump it there and it flows into the installer filename.

To build the Windows installer **locally** on a Windows machine instead:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DDOWNSLOPE_BUILD_TESTS=OFF
cmake --build build --config Release --target Downslope_VST3
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DMyAppVersion=0.1.0 packaging\windows\downslope.iss
```

---

## macOS installer (local)

```sh
cmake -B build -G Xcode
cmake --build build --config Release --target Downslope_AU Downslope_VST3
packaging/macos/build_pkg.sh
```

Produces `packaging/macos/Downslope-<version>-macOS.pkg`. Installs:

- AU  → `/Library/Audio/Plug-Ins/Components`
- VST3 → `/Library/Audio/Plug-Ins/VST3`

Both are user-selectable components in the installer UI. Installing to system
`/Library` requires admin, so the installer prompts for a password.

---

## Adding signing later

The scripts are structured so signing is an additive change, not a rewrite:

- **macOS**: sign the bundles with `codesign --sign "Developer ID Application: …"`
  before `pkgbuild`, sign the final `.pkg` with `productsign --sign
  "Developer ID Installer: …"`, then `xcrun notarytool submit` + `xcrun stapler
  staple`. Requires an Apple Developer ID.
- **Windows**: sign `Downslope-<version>-Windows.exe` with `signtool sign` using
  a code-signing certificate. Add a signing step after the Inno Setup step in
  `.github/workflows/windows.yml`.

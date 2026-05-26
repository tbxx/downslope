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

## Hosting downloads on S3 (public links)

One-time setup, then `publish_s3.sh` for each release. Replace `x09music-downloads`
with your globally-unique bucket name and pick a region (e.g. `us-east-1`).

**1. Install + configure the AWS CLI** (needs an IAM user access key with S3 write):
```sh
brew install awscli
aws configure   # enter access key, secret, default region
```

**2. Create the bucket and make objects publicly readable** (one time):
```sh
BUCKET=x09music-downloads
REGION=us-east-1

# create (us-east-1 takes no LocationConstraint; other regions do)
aws s3api create-bucket --bucket "$BUCKET" --region "$REGION"

# allow a public-read bucket policy (S3 blocks this by default)
aws s3api put-public-access-block --bucket "$BUCKET" \
  --public-access-block-configuration BlockPublicAcls=false,IgnorePublicAcls=false,BlockPublicPolicy=false,RestrictPublicBuckets=false

# grant anonymous read on all objects
aws s3api put-bucket-policy --bucket "$BUCKET" --policy '{
  "Version": "2012-10-17",
  "Statement": [{
    "Sid": "PublicRead",
    "Effect": "Allow",
    "Principal": "*",
    "Action": "s3:GetObject",
    "Resource": "arn:aws:s3:::x09music-downloads/*"
  }]
}'
```
ACLs are disabled on new buckets (bucket-owner-enforced), so the **bucket policy** —
not per-object ACLs — is what makes downloads public.

**3. Publish a release:**
```sh
S3_BUCKET=x09music-downloads packaging/publish_s3.sh
```
It uploads the built `.pkg` and `.exe` and prints the shareable URLs, e.g.
`https://x09music-downloads.s3.us-east-1.amazonaws.com/Downslope-0.1.0-macOS.pkg`.
(Build the macOS `.pkg` locally and drop the CI `.exe` into `packaging/windows/` first.)

> **Heads-up on unsigned + downloaded:** a `.pkg`/`.exe` fetched from the web gets
> a quarantine flag, so Gatekeeper/SmartScreen warnings are *stricter* than for a
> locally-built file — macOS users right-click → Open; Windows users click
> "More info → Run anyway". Code signing (below) removes this.

## Adding signing later

The scripts are structured so signing is an additive change, not a rewrite:

- **macOS**: sign the bundles with `codesign --sign "Developer ID Application: …"`
  before `pkgbuild`, sign the final `.pkg` with `productsign --sign
  "Developer ID Installer: …"`, then `xcrun notarytool submit` + `xcrun stapler
  staple`. Requires an Apple Developer ID.
- **Windows**: sign `Downslope-<version>-Windows.exe` with `signtool sign` using
  a code-signing certificate. Add a signing step after the Inno Setup step in
  `.github/workflows/windows.yml`.

# CA-2020 Chord Arpeggiator - build & install

Same routine as JNO-600 / SH-1010 / TR-9090.

## 1. Repository
1. github.com -> **+ -> New repository** -> name `CA2020`, **Private**, tick *Add a README* -> Create.
2. **Add file -> Upload files**: drag in the *contents* of the `CA2020` folder (CMakeLists.txt must be at the top level,
   next to the `Source`, `Resources`, `installer` folders). Commit.

## 2. Workflow file (hidden folder - do this by hand)
1. **Add file -> Create new file**, name: `.github/workflows/build.yml`
2. Open `build.yml.WORKFLOW-COPY.txt` from the package, copy everything, paste, **Commit**.
3. First line must read `name: Build CA-2020 v1`. Only one .yml file should be in `.github/workflows`.

## 3. Build
Actions tab -> **Build CA-2020 v1** runs by itself (about 10-15 min).
The step **Check files** must end with `All files found.`
Download **CA-2020-Windows** (Setup .exe inside) and optionally **CA-2020-macOS**.

If it fails: send me the lines containing `error C` (or the red step's last 30 lines).

## 4. Install
Run `CA2020-1.0.0-Windows-Setup.exe` -> app + VST3 (goes to `C:\Program Files\Common Files\VST3`).
In L-DAW: **Plug-ins -> Scan** so CA-2020 shows up.

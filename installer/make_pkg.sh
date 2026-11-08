#!/bin/bash
# Builds an (unsigned) macOS installer package: app, VST3 and AU
set -e
VER=1.0.0
ART=build/CA2020_artefacts/Release
STAGE=installer/stage
rm -rf "$STAGE"; mkdir -p "$STAGE/app" "$STAGE/vst3" "$STAGE/au"
cp -R "$ART/Standalone/CA-2020.app" "$STAGE/app/"
cp -R "$ART/VST3/CA-2020.vst3" "$STAGE/vst3/"
cp -R "$ART/AU/CA-2020.component" "$STAGE/au/"
pkgbuild --root "$STAGE/app"  --install-location /Applications                          --identifier com.landgrenaudio.ca2020.app  --version $VER installer/app.pkg
pkgbuild --root "$STAGE/vst3" --install-location /Library/Audio/Plug-Ins/VST3            --identifier com.landgrenaudio.ca2020.vst3 --version $VER installer/vst3.pkg
pkgbuild --root "$STAGE/au"   --install-location /Library/Audio/Plug-Ins/Components      --identifier com.landgrenaudio.ca2020.au   --version $VER installer/au.pkg
cat > installer/dist.xml <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>CA-2020 Chord Arpeggiator</title>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline><line choice="app"/><line choice="vst3"/><line choice="au"/></choices-outline>
  <choice id="app"  title="CA-2020 app"><pkg-ref id="com.landgrenaudio.ca2020.app"/></choice>
  <choice id="vst3" title="VST3 plug-in"><pkg-ref id="com.landgrenaudio.ca2020.vst3"/></choice>
  <choice id="au"   title="Audio Unit"><pkg-ref id="com.landgrenaudio.ca2020.au"/></choice>
  <pkg-ref id="com.landgrenaudio.ca2020.app">app.pkg</pkg-ref>
  <pkg-ref id="com.landgrenaudio.ca2020.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="com.landgrenaudio.ca2020.au">au.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution installer/dist.xml --package-path installer installer/CA2020-$VER-macOS.pkg
echo "Built installer/CA2020-$VER-macOS.pkg"

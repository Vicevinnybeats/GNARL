#!/usr/bin/env bash
#  GNARL - macOS installer (.pkg)
#
#  pkgbuild and productbuild ship with macOS, so this needs nothing
#  installed. Two components rather than one, because the AU and the VST3
#  live in different directories and a single payload rooted at
#  /Library/Audio/Plug-Ins would have to carry both trees anyway - this way
#  each one says where it goes.
#
#  UNSIGNED, deliberately and visibly. Gatekeeper will refuse it on first
#  open and the person has to right-click > Open, which is worse than a
#  signed package and better than a zip whose contents get quarantined one
#  file at a time. A Developer ID certificate (~99 USD/year) is what removes
#  this, and until there is one, saying so is more use than pretending.
set -euo pipefail

STAGE="${1:?usage: build-pkg.sh <stage-dir> <version> <output.pkg>}"
VERSION="${2:?}"
OUTPUT="${3:?}"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pkgs=()
#  Identifier per package, kept ALONGSIDE the path. Deriving it from the
#  filename looked tidy and was wrong: the standalone's package is app.pkg
#  but its identifier is com.gnarl.standalone, so the distribution would
#  have referenced a pkg-ref that does not exist and productbuild would
#  have failed on the one component nobody tests first.
ids=()

if [ -d "$STAGE/GNARL.vst3" ]; then
  mkdir -p "$work/vst3root/Library/Audio/Plug-Ins/VST3"
  cp -R "$STAGE/GNARL.vst3" "$work/vst3root/Library/Audio/Plug-Ins/VST3/"
  pkgbuild --root "$work/vst3root" --identifier com.gnarl.vst3 \
           --version "$VERSION" --install-location / "$work/vst3.pkg"
  pkgs+=("$work/vst3.pkg"); ids+=("com.gnarl.vst3")
fi

if [ -d "$STAGE/GNARL.component" ]; then
  mkdir -p "$work/auroot/Library/Audio/Plug-Ins/Components"
  cp -R "$STAGE/GNARL.component" "$work/auroot/Library/Audio/Plug-Ins/Components/"
  pkgbuild --root "$work/auroot" --identifier com.gnarl.au \
           --version "$VERSION" --install-location / "$work/au.pkg"
  pkgs+=("$work/au.pkg"); ids+=("com.gnarl.au")
fi

if [ -d "$STAGE/GNARL.app" ]; then
  mkdir -p "$work/approot/Applications"
  cp -R "$STAGE/GNARL.app" "$work/approot/Applications/"
  pkgbuild --root "$work/approot" --identifier com.gnarl.standalone \
           --version "$VERSION" --install-location / "$work/app.pkg"
  pkgs+=("$work/app.pkg"); ids+=("com.gnarl.standalone")
fi

if [ ${#pkgs[@]} -eq 0 ]; then
  echo "build-pkg: nothing to package in $STAGE" >&2
  exit 1
fi

#  A distribution wrapper, so the installer shows one product with a choice
#  of components rather than three separate packages.
{
  echo '<?xml version="1.0" encoding="utf-8"?>'
  echo '<installer-gui-script minSpecVersion="2">'
  echo "  <title>GNARL $VERSION</title>"
  echo '  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>'
  echo '  <choices-outline>'
  for i in "${!pkgs[@]}"; do
    echo "    <line choice=\"${ids[$i]}\"/>"
  done
  echo '  </choices-outline>'
  for i in "${!pkgs[@]}"; do
    echo "  <choice id=\"${ids[$i]}\" title=\"${ids[$i]##*.}\"><pkg-ref id=\"${ids[$i]}\"/></choice>"
    echo "  <pkg-ref id=\"${ids[$i]}\">$(basename "${pkgs[$i]}")</pkg-ref>"
  done
  echo '</installer-gui-script>'
} > "$work/distribution.xml"

productbuild --distribution "$work/distribution.xml" \
             --package-path "$work" "$OUTPUT"

echo "built $OUTPUT"

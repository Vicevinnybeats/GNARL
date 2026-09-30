#!/usr/bin/env python3
"""Fast guard for the fork. Runs before any platform build in CI.

EVERY CHECK HERE IS A BUG THAT ACTUALLY HAPPENED during the rebrand, which
is the only reason for each one to exist:

  - project files that stopped parsing: two separate regex edits left
    malformed XML (a quoted-token rule ate attributes; a `;REQUIRE_AUTH`
    rule ate the semicolon of an `&#10;` entity)
  - the plugin still identifying as upstream's Open/Vial, because the text
    rename could not see codes stored as hex
  - a five-character plugin code written where a FourCC takes four
  - upstream's personal email compiled into the binary as our contact
  - libraries that only existed inside the deleted Firebase SDK, still
    listed as link dependencies
  - logo bytes in BinaryData drifting from the SVG they came from
  - an Xcode project damaged by a regex (`name = ;`), invisible to an XML
    parser and fatal to xcodebuild

Exit status is the number of failures.
"""
import pathlib, re, sys, xml.dom.minidom

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pbxproj

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD_DIRS = ['plugin', 'standalone', 'headless', 'tests']
fails = []

def fail(msg): fails.append(msg); print('FAIL', msg)

def files(*globs):
    for d in BUILD_DIRS:
        for g in globs:
            yield from (ROOT / d).rglob(g)

def text(p): return p.read_text(encoding='utf-8', errors='surrogateescape')

# 1. every XML project file parses
for p in files('*.vcxproj', '*.filters', '*.jucer', '*.plist'):
    if '/build/' in str(p): continue
    try: xml.dom.minidom.parse(str(p))
    except Exception as e: fail(f'{p.relative_to(ROOT)} is not well-formed XML: {e}')

# 1b. every Xcode project parses. Xcode's format is NOT XML, so check 1
#     cannot see it: a regex left `name = ;` in two of these, and the guard
#     passed while xcodebuild refused the project with exit 74.
for p in files('project.pbxproj'):
    s = text(p)
    try: pbxproj.parse(s)
    except Exception as e: fail(f'{p.relative_to(ROOT)} is not a valid Xcode project: {e}')
    if re.search(r'^\s*"",', s, re.M):
        fail(f'{p.relative_to(ROOT)} has an empty "" list entry (a removed define left behind)')

# 2. identity: FourCC codes are four characters, and never upstream's
for p in files('*.jucer'):
    for key in ('pluginCode', 'pluginManufacturerCode'):
        for v in re.findall(r'%s="([^"]*)"' % key, text(p)):
            if len(v) != 4: fail(f'{p.relative_to(ROOT)}: {key}="{v}" is not a FourCC')
defines = text(ROOT / 'plugin/JuceLibraryCode/JucePluginDefines.h')
for old, name in (('0x4f70656e', 'Open'), ('0x5669616c', 'Vial')):
    if old in defines: fail(f'JucePluginDefines.h still identifies as upstream "{name}" ({old})')
for p in files('Info-*.plist'):
    s = text(p)
    for key in ('manufacturer', 'subtype'):
        for v in re.findall(r'<key>%s</key>\s*<string>([^<]*)</string>' % key, s):
            if len(v) != 4 or v in ('Open', 'Vial'):
                fail(f'{p.relative_to(ROOT)}: AU {key} "{v}"')

# 3. nothing of upstream's account system or identity in build files or source
banned = {'firebase': 'Firebase', 'matthewtytel': "upstream's personal email",
          'vital.audio': "upstream's domain", 'Vial': 'the old product name',
          'flatbuffers.lib': 'a library the deleted SDK shipped',
          'libcurl.lib': 'a library the deleted SDK shipped',
          #  Intel IPP is proprietary. Upstream may link it into Vital as the
          #  copyright holder; a GPLv3 binary WE distribute may not, and the
          #  runners do not have it either (C1083: ipps.h). Without it the FFT
          #  falls through to juce_dsp - the same path the Linux build takes.
          'INTEL_IPP': 'Intel IPP (proprietary; cannot be linked into our GPLv3 binary)',
          'UseIntelIPP': 'Intel IPP (proprietary; cannot be linked into our GPLv3 binary)',
          'IPPLibrary="Sequential"': 'Intel IPP (proprietary; cannot be linked into our GPLv3 binary)'}
allowed = {'src/common/authentication.h', 'src/interface/editor_sections/authentication_section.cpp'}
for p in list(files('*.vcxproj', '*.jucer', '*.pbxproj', 'Makefile*', '*.plist', '*.h', '*.rc')) + \
         [q for q in (ROOT / 'src').rglob('*') if q.suffix in ('.h', '.cpp')]:
    if '/build/' in str(p) or str(p.relative_to(ROOT)) in allowed: continue
    s = text(p)
    is_source = str(p.relative_to(ROOT)).startswith('src/')
    for word, why in banned.items():
        #  IPP is banned where a BUILD FILE switches it on. The source keeps
        #  its `#if INTEL_IPP` branch: that is the backend switch itself.
        if is_source and 'IPP' in word: continue
        if word in s: fail(f'{p.relative_to(ROOT)} mentions {why} ("{word}")')

# 4. the account system is compiled out everywhere
for p in files('AppConfig.h'):
    if not re.search(r'#define\s+NO_AUTH\s+1', text(p)):
        fail(f'{p.relative_to(ROOT)} does not define NO_AUTH 1')

# 5. embedded logo bytes match the SVGs they were made from - in EVERY
#    BinaryData. The plugin, the standalone app and the test runner each have
#    their own; checking only the plugin's let the standalone keep Vital's
#    logo bytes and stop compiling, unseen, because CI built only the VST3.
for hdr_path in sorted(ROOT.glob('*/JuceLibraryCode/BinaryData.h')):
    hdr = text(hdr_path)
    rel = hdr_path.relative_to(ROOT)
    if re.search(r'\bvital_(ring|v|word|word_ring)_svg\b', hdr):
        fail(f'{rel} still embeds Vital\'s logo - run tools/embed_logo.py')
    for sym in ('gnarl_ring_svg', 'gnarl_mark_svg', 'gnarl_word_svg', 'gnarl_word_ring_svg'):
        m = re.search(r'%sSize = (\d+);' % sym, hdr)
        svg = ROOT / 'icons' / (sym[:-4] + '.svg')
        if not m: fail(f'{rel} has no {sym}'); continue
        if int(m.group(1)) != svg.stat().st_size:
            fail(f'{rel}: {sym} is {m.group(1)} bytes, icons/{svg.name} is {svg.stat().st_size} - run tools/embed_logo.py')

# 6. no project references an icon file that does not exist
for p in files('*.jucer'):
    for ref in re.findall(r'file="\.\./icons/([^"]+)"', text(p)):
        if not (ROOT / 'icons' / ref).exists():
            fail(f'{p.relative_to(ROOT)} references icons/{ref}, which does not exist')

print(f'check_fork: {len(fails)} failure(s)')
sys.exit(min(len(fails), 100))

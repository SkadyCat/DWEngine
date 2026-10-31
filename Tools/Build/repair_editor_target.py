"""Recover Opus editor linking when the prebuilt engine lacks import libraries.

The installed engine ships DLLs but not every UnrealEditor-*.lib. For each project module that
UBT failed to link, this script rebuilds the missing engine import libraries from the genuine
DLL export tables (dumpbin + lib.exe), relinks the module with UBT's own response file, and asks
UBT to write the real target receipt. The default mode writes only project files.
--restore-sdk additionally fills missing engine Intermediate import .lib inputs;
it never overwrites an existing file or rebuilds an engine DLL. UBT can then link
normally and keep unchanged builds up to date.

Usage: python Tools/Build/repair_editor_target.py [--check | --restore-sdk]
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
assert (ROOT / 'DungeonWorldOpus.uproject').is_file()
ENGINE = Path('G:/UnrealEngine-Angelscript-5.8.3/Engine')
TOOL = Path('G:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64')
META = ROOT / 'Intermediate/Build/Win64/x64/DungeonWorldOpusEditor/Development/TargetMetadata.json'
OUT = ROOT / 'Saved/EditorTargetRepair' / str(time.time_ns())
SDK = ROOT / 'Saved/EditorTargetRepair/EditorSDK'  # shared cache of generated import libs

ap = argparse.ArgumentParser()
modes = ap.add_mutually_exclusive_group()
modes.add_argument('--check', action='store_true')
modes.add_argument('--restore-sdk', action='store_true')
args = ap.parse_args()
OUT.mkdir(parents=True, exist_ok=True)
SDK.mkdir(parents=True, exist_ok=True)
meta = json.loads(META.read_text(encoding='utf8'))


def import_lib(lib: Path) -> Path:
    provider = ENGINE / 'Binaries/Win64' / (lib.stem + '.dll')
    if not provider.is_file():
        provider = next((p / 'Binaries/Win64' / (lib.stem + '.dll') for p in lib.parents
                         if (p / 'Binaries/Win64' / (lib.stem + '.dll')).is_file()), provider)
    assert provider.is_file(), 'No installed DLL for ' + str(lib)
    local = SDK / (lib.stem + '.lib')
    if not local.is_file() or local.stat().st_mtime_ns < provider.stat().st_mtime_ns:
        exports = subprocess.check_output([str(TOOL / 'dumpbin.exe'), '/exports', str(provider)],
                                          text=True, encoding='utf8', errors='replace')
        symbols = re.findall(r'^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)', exports, re.M)
        assert symbols, 'No exports ' + str(provider)
        definition = local.with_suffix('.def')
        definition.write_text('LIBRARY ' + provider.name + '\nEXPORTS\n' + '\n'.join(symbols) + '\n', encoding='utf8')
        with local.with_suffix('.log').open('w', encoding='utf8') as f:
            subprocess.run([str(TOOL / 'lib.exe'), '/nologo', '/machine:x64', '/def:' + str(definition), '/out:' + str(local)],
                           check=True, stdout=f, stderr=subprocess.STDOUT)
    return local


restored = []


def restore_sdk_import(lib: Path, local: Path):
    destination = lib.resolve()
    assert destination.is_relative_to(ENGINE.resolve()), 'Not an engine input: ' + str(lib)
    parts = [part.lower() for part in destination.parts]
    assert any(parts[i:i + 3] == ['intermediate', 'build', 'win64']
               for i in range(len(parts) - 2)), 'Not a generated Win64 SDK input: ' + str(lib)
    assert destination.name.startswith('UnrealEditor-') and destination.suffix == '.lib'
    destination.parent.mkdir(parents=True, exist_ok=True)
    staged = OUT / ('sdk-' + destination.name)
    shutil.copy2(local, staged)
    try:
        # On Windows rename is atomic and refuses to replace an existing file.
        staged.rename(destination)
    except FileExistsError:
        # Another build may have restored the same input; never replace it.
        assert destination.read_bytes() == local.read_bytes(), 'Existing SDK input differs: ' + str(lib)
    restored.append({'path': str(destination), 'source': str(local),
                     'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()})
    print('RESTORED SDK import', destination, flush=True)


def find_rsp(name: str, dll: Path):
    pats = ['Intermediate/Build/Win64/x64/UnrealEditor/Development/' + name + '/' + dll.stem + '.dll.rsp',
            'Plugins/*/Intermediate/Build/Win64/x64/UnrealEditor/Development/' + name + '/' + dll.stem + '.dll.rsp',
            'Plugins/*/*/Intermediate/Build/Win64/x64/UnrealEditor/Development/' + name + '/' + dll.stem + '.dll.rsp']
    found = [p for pat in pats for p in ROOT.glob(pat)]
    assert len(found) == 1, (name, found)
    return found[0]


plan = []
for manifest, info in meta['FileToManifest'].items():
    base = Path(manifest).parent
    assert base.resolve().is_relative_to(ROOT)
    for name, dllname in info['ModuleNameToFileName'].items():
        dll = base / dllname
        rsp = find_rsp(name, dll)
        text = rsp.read_text(encoding='utf-8-sig')
        patched = False
        for line in text.splitlines():
            raw = line.strip().strip('"')
            if raw.lower().endswith('.obj'):
                obj = Path(raw)
                if not obj.is_absolute():
                    obj = ENGINE / 'Source' / obj
                assert obj.is_file(), 'Missing compiled object ' + str(obj)
            if not raw.lower().endswith('.lib') or '/' not in raw.replace('\\', '/'):
                continue
            lib = Path(raw)
            if not lib.is_absolute():
                lib = (ENGINE / 'Source' / lib).resolve()
            if lib.is_file():
                continue
            assert lib.resolve().is_relative_to(ENGINE), 'Missing project input ' + str(lib)
            local = import_lib(lib)
            if args.restore_sdk:
                restore_sdk_import(lib, local)
            text = text.replace(raw, local.as_posix())
            patched = True
        out_rsp = OUT / (name + '.link.rsp')
        out_rsp.write_text(text, encoding='utf8')
        plan.append({'name': name, 'dll': str(dll), 'rsp': str(out_rsp), 'patched': patched})

print('Verified objects and import libs for', len(plan), 'project modules', flush=True)
if args.check:
    raise SystemExit(0)
if args.restore_sdk:
    report = OUT / 'restored-engine-imports.json'
    report.write_text(json.dumps(restored, indent=2), encoding='utf8')
    print('SDK restore report', report, flush=True)
    raise SystemExit(0)

# Link in dependency-safe order: modules whose import .lib other modules need are produced by
# the first link of each module (link.exe emits the .lib next to the DLL as /IMPLIB).
pending = list(plan)
for attempt in range(4):
    failed = []
    for item in pending:
        dll = Path(item['dll'])
        if dll.is_file():
            backup = OUT / 'BinariesBackup' / dll.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dll, backup)
        log = OUT / (item['name'] + '.link.log')
        with log.open('w', encoding='utf8') as f:
            rc = subprocess.run([str(TOOL / 'link.exe'), '@' + item['rsp']], cwd=ENGINE / 'Source', stdout=f, stderr=subprocess.STDOUT).returncode
        if rc != 0:
            failed.append(item)
        else:
            print('LINKED', item['name'], flush=True)
    if not failed:
        break
    print('retry', [i['name'] for i in failed], flush=True)
    pending = failed
else:
    for item in pending:
        print('FAILED', item['name'], (OUT / (item['name'] + '.link.log')).read_text(encoding='utf8', errors='replace')[-1500:])
    raise SystemExit(1)

engine_id = json.loads((ENGINE / 'Binaries/Win64/UnrealEditor.modules').read_text(encoding='utf8'))['BuildId']
meta['Receipt']['Version']['BuildId'] = engine_id
assert Path(meta['ReceiptFile']).resolve().is_relative_to(ROOT)
meta['VersionFile'] = None
preserved = OUT / 'TargetMetadata.project.json'
preserved.write_text(json.dumps(meta, indent=2), encoding='utf8')
version_source = ENGINE / 'Source/Programs/UnrealBuildTool/Modes/WriteMetadataMode.cs'
version = re.search(r'CurrentVersionNumber\s*=\s*(\d+)', version_source.read_text(encoding='utf8')).group(1)
dotnet = ENGINE / 'Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe'
with (OUT / 'write-metadata.log').open('w', encoding='utf8') as log:
    subprocess.run([str(dotnet), str(ENGINE / 'Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'), '-Mode=WriteMetadata',
                    '-Input=' + str(preserved), '-Version=' + version], cwd=ENGINE / 'Source', check=True, stdout=log, stderr=subprocess.STDOUT)
print('Receipt written', meta['ReceiptFile'], flush=True)

"""Publish UBT's existing module-build receipt without writing engine metadata."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("project", type=Path)
parser.add_argument("engine", type=Path)
parser.add_argument("--built-project", type=Path)
args = parser.parse_args()
target = args.project.resolve()
built = (args.built_project or target).resolve()
engine = args.engine.resolve()
metadata_path = built / "Intermediate/Build/Win64/x64/SvgWbpLabEditor/Development/TargetMetadata.json"
metadata = json.loads(metadata_path.read_text(encoding="utf-8-sig"))
build_id = json.loads((engine / "Engine/Binaries/Win64/UnrealEditor.modules").read_text(encoding="utf-8-sig"))["BuildId"]

for manifest_name, manifest in metadata["FileToManifest"].items():
    relative = Path(manifest_name).relative_to(built)
    installed_manifest = json.loads((target / relative).read_text(encoding="utf-8-sig"))
    if installed_manifest["BuildId"] != build_id:
        raise RuntimeError("The installed modules do not match the engine build")
    for module, filename in manifest["ModuleNameToFileName"].items():
        source = Path(manifest_name).parent / filename
        destination = (target / relative).parent / filename
        if hashlib.sha256(source.read_bytes()).digest() != hashlib.sha256(destination.read_bytes()).digest():
            raise RuntimeError(f"Installed module differs from the validated build: {module}")

def relocate(value):
    if isinstance(value, str):
        return value.replace(str(built), str(target))
    if isinstance(value, list):
        return [relocate(item) for item in value]
    if isinstance(value, dict):
        return {relocate(key): relocate(item) for key, item in value.items()}
    return value

metadata = relocate(metadata)
metadata["VersionFile"] = None
metadata["Version"] = None
metadata["Receipt"]["Version"]["BuildId"] = build_id
metadata["FileToManifest"] = {}
metadata["FileToLoadOrderManifest"] = {}
output = target / "Binaries/Win64/ModuleBuildReceipt.json"
output.write_text(json.dumps(metadata, ensure_ascii=False), encoding="utf-8")
print(output)

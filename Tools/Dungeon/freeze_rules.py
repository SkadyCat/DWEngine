"""Materialize the user-selected runnable baseline without editing incoming rule work."""
import hashlib
import io
import json
import subprocess
import tarfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
RECEIPT=ROOT/'Docs/ZeroDungeon/runtime-rules.json'
BASELINE='e2abf77cf5a00f75a601aaefa71e7b1945eaaaae'
commit=json.loads(RECEIPT.read_text('utf8'))['sourceCommit'] if RECEIPT.exists() else BASELINE
assert commit==BASELINE,'Change the accepted baseline explicitly before rebuilding this snapshot'
archive=subprocess.check_output(['git','-C',str(ROOT),'archive','--format=tar',commit,'Content/Rules'])
entries=[]
with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
    for member in tar.getmembers():
        if member.isdir():continue
        assert member.isfile() and member.name.startswith('Content/Rules/'),member.name
        rel=Path(member.name.removeprefix('Content/Rules/'))
        dst=(ROOT/'Content/RulesTower'/rel).resolve()
        assert dst.is_relative_to((ROOT/'Content/RulesTower').resolve())
        data=tar.extractfile(member).read()
        if dst.exists():assert dst.read_bytes()==data,'Refusing overwrite '+str(dst)
        else:
            dst.parent.mkdir(parents=True,exist_ok=True)
            dst.write_bytes(data)
        entries.append({'file':rel.as_posix(),'sha256':hashlib.sha256(data).hexdigest()})
RECEIPT.write_text(json.dumps({'sourceCommit':commit,'runtimeRoot':'Content/RulesTower','userDecision':'Only dungeon; retain the previously runnable rules. Incoming Content/Rules authoring work remains separate.','files':entries},ensure_ascii=False,indent=2),'utf8')
print('FROZEN_RULES_PASS commit=%s files=%d' % (commit,len(entries)))

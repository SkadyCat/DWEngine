"""Read-only source asset audit and exact dependency migration into this lab."""
import hashlib
import json
import pathlib
import shutil
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path('G:/DungeonWorldOpus/Content')
MODELS = json.loads((ROOT / 'Config/models.json').read_text(encoding='utf-8'))
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
registry.wait_for_completion()
options = unreal.AssetRegistryDependencyOptions(
    include_hard_package_references=True, include_soft_package_references=True,
    include_searchable_names=False, include_hard_management_references=False,
    include_soft_management_references=False)
pending = [m['mesh'] for m in MODELS]
pending += [p for m in MODELS for p in m['clips'].values()]
seen = set()
while pending:
    package = pending.pop().split('.')[0]
    if package in seen or not package.startswith('/Game/'):
        continue
    seen.add(package)
    pending.extend(str(p) for p in registry.get_dependencies(package, options))
copied = []
for package in sorted(seen):
    relative = pathlib.Path(package.removeprefix('/Game/'))
    for ext in ('.uasset', '.uexp', '.ubulk'):
        source = SOURCE / (str(relative) + ext)
        if not source.exists():
            continue
        dest = ROOT / 'Content' / source.relative_to(SOURCE)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
        copied.append({'file':dest.relative_to(ROOT).as_posix(), 'bytes':source.stat().st_size,
                       'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})

def vector(v):
    return [v.x, v.y, v.z]

audits = []
for model in MODELS:
    mesh = unreal.load_asset(model['mesh'])
    if mesh is None:
        raise RuntimeError('Missing mesh: ' + model['mesh'])
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    skeleton = mesh.get_editor_property('skeleton')
    def choose(candidates):
        lowered = {n.lower():n for n in names}
        return next((lowered[n.lower()] for n in candidates if n.lower() in lowered), '')
    model['leftFoot'] = choose(['foot_l','LeftFoot','mixamorig:LeftFoot','foot.L','Bip001-L-Foot','l_foot'])
    model['rightFoot'] = choose(['foot_r','RightFoot','mixamorig:RightFoot','foot.R','Bip001-R-Foot','r_foot'])
    model['pelvis'] = choose(['pelvis','Hips','mixamorig:Hips','hip','Bip001-Pelvis'])
    report = {'id':model['id'], 'mesh':model['mesh'], 'skeleton':skeleton.get_path_name(),
              'bones':names, 'leftFoot':model['leftFoot'], 'rightFoot':model['rightFoot'], 'clips':{}}
    for state, path in model['clips'].items():
        clip = unreal.load_asset(path)
        if clip is None:
            raise RuntimeError('Missing animation: ' + path)
        clip_skeleton = clip.get_editor_property('skeleton')
        length = unreal.AnimationLibrary.get_sequence_length(clip)
        item = {'asset':path, 'skeleton':clip_skeleton.get_path_name(),
                'sameSkeleton':clip_skeleton == skeleton, 'duration':length,
                'rootMotion':clip.get_editor_property('enable_root_motion')}
        try:
            tracks = unreal.AnimationLibrary.get_animation_track_names(clip)
            root = str(tracks[0])
            samples = []
            for t in [0, length*.25, length*.5, length*.75, max(0,length-.001)]:
                pose = unreal.AnimationLibrary.get_bone_pose_for_time(clip, root, t, False)
                samples.append(vector(pose.translation))
            item.update(rootTrack=root, rootSamples=samples)
        except Exception as error:
            item['rootSampleError'] = str(error)
        report['clips'][state] = item
    audits.append(report)
(ROOT/'Docs').mkdir(exist_ok=True)
(ROOT/'Docs/source-audit.json').write_text(json.dumps(audits,ensure_ascii=False,indent=2),encoding='utf8')
(ROOT/'Docs/asset-manifest.json').write_text(json.dumps(copied,ensure_ascii=False,indent=2),encoding='utf8')
(ROOT/'Config/models.json').write_text(json.dumps(MODELS,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('C3_SOURCE_EXPORT_PASS models=%d files=%d' % (len(MODELS),len(copied)))

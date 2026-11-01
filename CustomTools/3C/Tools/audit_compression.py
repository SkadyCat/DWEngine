"""Compare raw poses to the compressed poses used by normal runtime playback."""
import json
import pathlib
import unreal
ROOT=pathlib.Path(__file__).resolve().parents[1]
models=json.loads((ROOT/'Config/models.json').read_text(encoding='utf8'))
result=[]
for model in models:
    mesh=unreal.load_asset(model['mesh'])
    raw=unreal.AnimPoseEvaluationOptions(optional_skeletal_mesh=mesh,extract_root_motion=True,incorporate_root_motion_into_pose=False)
    compressed=unreal.AnimPoseEvaluationOptions(optional_skeletal_mesh=mesh,extract_root_motion=True,incorporate_root_motion_into_pose=False,evaluation_type=unreal.AnimDataEvalType.COMPRESSED)
    states={}
    for state in ('idle','run','attack1','hit','death'):
        clip=unreal.load_asset(model['clips'][state]);length=unreal.AnimationLibrary.get_sequence_length(clip)
        values=[]
        for ratio in (0,.25,.5,.75,.999):
            a=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,length*ratio,raw)
            b=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,length*ratio,compressed)
            for bone in ('pelvis','head',model['leftFoot'],model['rightFoot']):
                x=unreal.AnimPoseExtensions.get_bone_pose(a,bone,unreal.AnimPoseSpaces.WORLD).translation
                y=unreal.AnimPoseExtensions.get_bone_pose(b,bone,unreal.AnimPoseSpaces.WORLD).translation
                values.append({'phase':ratio,'bone':bone,'distance':(x-y).length(),'raw':[x.x,x.y,x.z],'compressed':[y.x,y.y,y.z]})
        states[state]=values
    result.append({'id':model['id'],'clips':states})
(ROOT/'Docs/compression-audit.json').write_text(json.dumps(result,indent=2),encoding='utf8')
unreal.log('C3_COMPRESSION_AUDIT_PASS models='+str(len(result)))

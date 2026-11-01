"""Measure each locomotion clip's planted-foot travel on its real target mesh."""
import json
import math
import pathlib
import statistics
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
models = json.loads((ROOT/'Config/models.json').read_text(encoding='utf8'))
output = []
for model in models:
    mesh = unreal.load_asset(model['mesh'])
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', mesh)
    options.set_editor_property('extract_root_motion', True)
    options.set_editor_property('incorporate_root_motion_into_pose', False)
    clips = {}
    for state in ('run', 'walk'):
        if state not in model['clips']:
            continue
        clip = unreal.load_asset(model['clips'][state])
        length = unreal.AnimationLibrary.get_sequence_length(clip)
        count = 180
        points = [[], []]
        for index in range(count+1):
            t = min(length-.00001, length*index/count)
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, t, options)
            for foot, bone in enumerate((model['leftFoot'], model['rightFoot'])):
                p = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
                points[foot].append([p.x, p.y, p.z])
        dt = length/count
        candidates = []
        directions = []
        contacts = []
        for foot, values in enumerate(points):
            low = min(p[2] for p in values)
            for index in range(1, count):
                a, b = values[index-1], values[index+1]
                velocity = [(b[j]-a[j])/(2*dt) for j in range(3)]
                # The source meshes face local +Y; stance travels towards -Y.
                if values[index][2] <= low+7 and abs(velocity[2]) < 50 and velocity[1] < -15:
                    candidates.append(-velocity[1])
                    directions.append(velocity[:2])
                    contacts.append({'foot':foot, 'phase':index/count, 'height':values[index][2], 'speed':-velocity[1]})
        stride_span=max(max(p[1] for p in values)-min(p[1] for p in values) for values in points)
        non_striding=len(candidates)<8 and stride_span<10
        if len(candidates) < 8 and not non_striding:
            raise RuntimeError('Insufficient contact samples: '+model['id']+' '+state)
        speed = model['gait'] if non_striding else statistics.median(candidates)
        clips[state] = {'speed':speed, 'duration':length, 'contactSamples':len(candidates),
                        'striding':not non_striding, 'foreAftSpan':stride_span,
                        'minimumFootZ':[min(p[2] for p in values) for values in points],
                        'p10':sorted(candidates)[int(len(candidates)*.1)] if candidates else None,
                        'p90':sorted(candidates)[int(len(candidates)*.9)] if candidates else None,
                        'meanLateralSpeed':statistics.mean(abs(v[0]) for v in directions) if directions else 0,
                        'contacts':contacts}
    output.append({'id':model['id'], 'clips':clips})
(ROOT/'Docs/gait-calibration.json').write_text(json.dumps(output, indent=2), encoding='utf8')
lines=['// Generated from evaluated native foot-contact poses; cm/s at play rate 1.',
       'namespace C3Gaits {', ' double Speed(FString Model,FName State) {']
for model in output:
    values=model['clips']
    walk=values.get('walk',values['run'])['speed']
    lines.append('  if(Model=="%s")return State==n"walk"?%.9f:%.9f;'%(model['id'],walk,values['run']['speed']))
lines.extend(['  return 300;', ' }', '}'])
non_striding=[m['id'] for m in output if not m['clips']['run']['striding']]
condition='&&'.join('Model!="%s"'%name for name in non_striding) or 'true'
lines[-1:-1]=[' bool Striding(FString Model) {return '+condition+';}']
(ROOT/'Script/Gaits.as').write_text('\n'.join(lines)+'\n', encoding='utf8')
unreal.log('C3_GAIT_CALIBRATION_PASS models=%d clips=%d'%(len(output),sum(len(m['clips']) for m in output)))

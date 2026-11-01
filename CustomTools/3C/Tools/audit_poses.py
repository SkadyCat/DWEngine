"""Evaluate copied source animation poses on their actual target meshes."""
import json
import math
import pathlib
import unreal

ROOT=pathlib.Path(__file__).resolve().parents[1]
models=json.loads((ROOT/'Config/models.json').read_text(encoding='utf8'))
out=[]
for model in models:
    mesh=unreal.load_asset(model['mesh'])
    materials=[]
    for slot in mesh.get_editor_property('materials'):
        interface=slot.material_interface
        if interface:
            base=interface.get_base_material()
            try:
                wpo=unreal.MaterialEditingLibrary.get_material_property_input_node(base,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
                wpo=wpo.get_class().get_name() if wpo else None
            except Exception as error:wpo='UNAVAILABLE: '+str(error)
            materials.append({'interface':interface.get_path_name(),'base':base.get_path_name(),'worldPositionOffset':wpo})
    result={'id':model['id'],'materials':materials,'clips':{}}
    options=unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh',mesh)
    options.set_editor_property('extract_root_motion',True)
    options.set_editor_property('incorporate_root_motion_into_pose',False)
    for state,path in model['clips'].items():
        clip=unreal.load_asset(path)
        length=unreal.AnimationLibrary.get_sequence_length(clip)
        scale_min=1e9;scale_max=-1e9;stretch=[];feet=[];bones=[]
        for sample in range(9):
            t=min(length-.0001,length*sample/8)
            pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,max(0,t),options)
            if not unreal.AnimPoseExtensions.is_valid(pose):raise RuntimeError('Invalid evaluated pose: '+path)
            bones=unreal.AnimPoseExtensions.get_bone_names(pose)
            for bone in bones:
                local=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.LOCAL)
                reference=unreal.AnimPoseExtensions.get_ref_bone_pose(pose,bone,unreal.AnimPoseSpaces.LOCAL)
                s=local.scale3d
                scale_min=min(scale_min,s.x,s.y,s.z);scale_max=max(scale_max,s.x,s.y,s.z)
                current=local.translation.length();rest=reference.translation.length()
                if rest>3 and (current/rest>1.4 or current/rest<0.6):
                    stretch.append({'sample':sample,'bone':str(bone),'ratio':current/rest})
            foot=[]
            for bone in (model['leftFoot'],model['rightFoot']):
                p=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
                foot.append([p.translation.x,p.translation.y,p.translation.z])
            feet.append({'t':max(0,t),'left':foot[0],'right':foot[1]})
        result['clips'][state]={'duration':length,'bones':len(bones),'scaleMin':scale_min,'scaleMax':scale_max,'boneLengthOutliers':stretch,'feet':feet}
    out.append(result)
(ROOT/'Docs/pose-audit.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('C3_POSE_AUDIT_PASS models=%d clips=%d' % (len(out),sum(len(m['clips']) for m in out)))

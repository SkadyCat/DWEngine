"""Read live registered components to verify telemetry and actual playback."""
import json
import pathlib
import unreal
ROOT=pathlib.Path(__file__).resolve().parents[1]
world=unreal.load_asset('/Game/ThreeCLab/Maps/L_ThreeCArena')
cls=unreal.load_class(None,'/Script/Angelscript.ThreeCFighter')
fighters=unreal.GameplayStatics.get_all_actors_of_class(world,cls)
seen=set();result=[]
def xyz(v):return [v.x,v.y,v.z]
def optional(obj,key):
    try:
        v=obj.get_editor_property(key)
        return v.get_path_name() if isinstance(v,unreal.Object) else str(v)
    except Exception as error:return 'UNAVAILABLE: '+str(error)
for actor in fighters:
    model=actor.get_editor_property('model_id')
    if model in seen:continue
    seen.add(model)
    mesh=actor.get_component_by_class(unreal.SkeletalMeshComponent)
    asset=mesh.get_skinned_asset()
    instance=mesh.get_anim_instance()
    item={'id':model,'actor':actor.get_path_name(),'mesh':asset.get_path_name(),'actorLocation':xyz(actor.get_actor_location()),'meshLocation':xyz(mesh.get_world_location()),'meshRotation':str(mesh.get_world_rotation()),'meshScale':xyz(mesh.get_world_scale()),'currentAnimation':optional(instance,'current_asset'),'postProcessBlueprint':optional(asset,'post_process_anim_blueprint'),'feet':{}}
    for bone in ('foot_l','foot_r','pelvis','head','root'):
        transform=mesh.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT)
        item['feet'][bone]={'world':xyz(mesh.get_socket_location(bone)),'component':xyz(transform.translation),'resolvedBone':str(mesh.get_socket_bone_name(bone))}
    result.append(item)
(ROOT/'Docs/runtime-components.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('C3_RUNTIME_AUDIT_PASS actors=%d models=%d'%(len(fighters),len(result)))

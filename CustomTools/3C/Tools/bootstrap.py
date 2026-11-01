"""Create the actual editor arena, team materials, and 100 placed AS fighters."""
import json
import math
import pathlib
import unreal

ROOT=pathlib.Path(__file__).resolve().parents[1]
MODELS=json.loads((ROOT/'Config/models.json').read_text(encoding='utf8'))
MAP='/Game/ThreeCLab/Maps/L_ThreeCArena'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets=unreal.AssetToolsHelpers.get_asset_tools()
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if world is None:raise RuntimeError('Failed to create the dedicated lab world')
def material(name,color):
    path='/Game/ThreeCLab/Materials/'+name
    mat=unreal.load_asset(path)
    if mat is None:
        mat=assets.create_asset(name,'/Game/ThreeCLab/Materials',unreal.Material,unreal.MaterialFactoryNew())
        p=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,0,0)
        p.set_editor_property('constant',unreal.LinearColor(*color,1))
        unreal.MaterialEditingLibrary.connect_material_property(p,'',unreal.MaterialProperty.MP_BASE_COLOR)
        rough=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant,-150,180)
        rough.set_editor_property('r',0.85)
        unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat
floor_mat=material('M_Floor',(0.22,0.25,0.29))
team_mat=[material('M_Team%d'%i,c) for i,c in enumerate([(0.85,0.07,0.035),(0.035,0.25,0.9),(0.05,0.8,0.16)])]
stripe_mat=material('M_Grid',(0.055,0.07,0.09))
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
def block(label,location,scale,mat,rotation=None):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*location),rotation or unreal.Rotator())
    a.set_actor_label(label)
    a.static_mesh_component.set_static_mesh(cube)
    a.static_mesh_component.set_material(0,mat)
    a.set_actor_scale3d(unreal.Vector(*scale))
    return a
block('Arena Floor',(0,0,-40),(80,80,0.8),floor_mat)
for i in range(-8,9):
    block('Grid X %d'%i,(i*500,0,0.2),(.012,80,.005),stripe_mat)
    block('Grid Y %d'%i,(0,i*500,0.2),(80,.012,.005),stripe_mat)
for i in range(4):
    block('Step %d'%i,(-2900+i*100,1000,5*(i+1)),(1.0,5,.1*(i+1)),floor_mat)
block('Grounding Ramp',(-2300,1000,30),(6,5,.15),floor_mat,unreal.Rotator(12,0,0))
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1500),unreal.Rotator(-55,-30,0))
sun.light_component.set_editor_property('intensity',4.5)
sun.light_component.set_forward_shading_priority(1)
sun.light_component.set_editor_property('light_color',unreal.Color(255,246,225,255))
sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1200))
sky.light_component.set_editor_property('intensity',0.75)
actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(0,0,200))
fill_class=unreal.load_class(None,'/Script/Angelscript.ThreeCSceneFill')
actors.spawn_actor_from_class(fill_class,unreal.Vector(0,0,1200))
cls=unreal.load_class(None,'/Script/Angelscript.ThreeCFighter')
if cls is None:raise RuntimeError('AS fighter class missing')
for i in range(100):
    team=i%3;slot=i//3;angle=team*2*math.pi/3
    pos=unreal.Vector(math.cos(angle)*1850+(slot%6-2.5)*150,math.sin(angle)*1850+(slot//6-2.5)*150,85)
    model=MODELS[(i//3+team*5)%len(MODELS)]
    actor=actors.spawn_actor_from_class(cls,pos,unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(-pos.y,-pos.x)),roll=0))
    actor.set_actor_label('Faction%d_%03d_%s'%(team+1,i,model['id']))
    actor.set_editor_property('index',i)
    actor.set_editor_property('team',team)
    actor.set_editor_property('model_id',model['id'])
    mesh=actor.get_component_by_class(unreal.SkeletalMeshComponent)
    mesh.set_skeletal_mesh_asset(unreal.load_asset(model['mesh']))
    mesh.set_relative_scale3d(unreal.Vector(model['scale'],model['scale'],model['scale']))
    mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    mesh.play_animation(unreal.load_asset(model['clips']['idle']),True)
    mesh.set_update_animation_in_editor(True)
    marker=actor.get_component_by_class(unreal.StaticMeshComponent)
    marker.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cylinder'))
    marker.set_material(0,team_mat[team])
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Script/Angelscript.ThreeCGameMode'))
if not unreal.EditorLoadingAndSavingUtils.save_map(world,MAP):raise RuntimeError('Failed to save arena')
unreal.EditorAssetLibrary.save_directory('/Game/ThreeCLab',only_if_is_dirty=True,recursive=True)
count=sum(1 for a in actors.get_all_level_actors() if a.get_class()==cls)
if count!=100:raise RuntimeError('Placed count mismatch: '+str(count))
(ROOT/'Saved/Diagnostics').mkdir(exist_ok=True)
(ROOT/'Docs/map-receipt.json').write_text(json.dumps({'map':MAP,'fighters':count,'factions':[34,33,33],'models':len(MODELS)},indent=2),encoding='utf8')
unreal.log('C3_MAP_PASS fighters=100 factions=34/33/33 models=15 saved='+MAP)

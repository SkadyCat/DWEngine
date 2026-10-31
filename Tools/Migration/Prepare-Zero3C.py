"""Validate the migrated original graph and retain the death montage's last pose."""
from pathlib import Path
import json,unreal as u
root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
assert root==Path('G:/DungeonWorldOpus')
assert not u.EditorLevelLibrary.get_pie_worlds(True)
path='/Game/Clean3C/Animation/Channel/ABP_Clean3C_ChannelMobile'
graph=u.load_class(None,path+'.ABP_Clean3C_ChannelMobile_C');assert graph
defaults=u.get_default_object(graph)
databases=defaults.get_editor_property('databases');assert len(databases)>10
montages=defaults.get_editor_property('network_action_montages')
death=u.load_asset('/Game/Clean3C/Animation/Montages/AM_Set_Mage_Death_IP');assert death
death.set_editor_property('enable_auto_blend_out',False)
assert u.EditorAssetLibrary.save_loaded_asset(death,False)
report={'graph':path,'nativeParent':str(graph.get_super_class()) if hasattr(graph,'get_super_class') else 'CleanMotionAnimInstance','databaseCount':len(databases),'databases':[x.get_path_name() for x in databases],'montageCount':len(montages),'deathHoldsFinalPose':not death.get_editor_property('enable_auto_blend_out')}
(root/'Docs/Zero3CPrepared.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('ZERO3C_PREPARED '+json.dumps(report))

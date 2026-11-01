"""Author presentation-only montages; source sequences and skeletons are untouched."""
import json
import pathlib
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
models = json.loads((ROOT / 'Config/models.json').read_text(encoding='utf8'))
tools = unreal.AssetToolsHelpers.get_asset_tools()
receipt = []
for model in models:
    for state, source in model['clips'].items():
        sequence = unreal.load_asset(source)
        path = '/Game/ThreeCLab/Animation/' + model['id']
        name = 'AM_' + state
        montage = unreal.load_asset(path + '/' + name)
        if montage is None:
            factory = unreal.AnimMontageFactory()
            factory.set_editor_property('source_animation', sequence)
            factory.set_editor_property('target_skeleton', sequence.get_editor_property('skeleton'))
            montage = tools.create_asset(name, path, unreal.AnimMontage, factory)
        assert montage is not None, (model['id'], state)
        montage.set_editor_property('enable_auto_blend_out', False)
        for prop in ('blend_in', 'blend_out'):
            blend = montage.get_editor_property(prop)
            blend.set_editor_property('blend_time', .22)
            blend.set_editor_property('blend_option', unreal.AlphaBlendOption.CUBIC)
            montage.set_editor_property(prop, blend)
        assert unreal.EditorAssetLibrary.save_loaded_asset(montage), montage.get_path_name()
        assert montage.get_editor_property('enable_auto_blend_out') is False
        tracks = montage.get_editor_property('slot_anim_tracks')
        assert str(tracks[0].get_editor_property('slot_name')) == 'DefaultSlot'
        receipt.append({'model': model['id'], 'state': state, 'montage': montage.get_path_name(),
                        'sequence': source, 'holdLastPose': True, 'blendSeconds': .22})
(ROOT / 'Docs/montage-assets.json').write_text(json.dumps(receipt, indent=2), encoding='utf8')
unreal.log('C3_MONTAGE_ASSETS_PASS count=' + str(len(receipt)))

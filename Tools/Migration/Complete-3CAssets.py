"""UE Python: fill montage presentation coverage for Opus's other lineages."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/Verification/ThreeC'
models = json.loads((OUT / 'models.json').read_text(encoding='utf8'))
tools = unreal.AssetToolsHelpers.get_asset_tools()
receipt = []
for model in models:
    for state, source in model['clips'].items():
        sequence = unreal.load_asset(source)
        assert sequence is not None, source
        folder = '/Game/ThreeCLab/Animation/' + model['id']
        name = 'AM_' + state
        montage = unreal.load_asset(folder + '/' + name)
        created = montage is None
        if created:
            factory = unreal.AnimMontageFactory()
            factory.set_editor_property('source_animation', sequence)
            factory.set_editor_property('target_skeleton', sequence.get_editor_property('skeleton'))
            montage = tools.create_asset(name, folder, unreal.AnimMontage, factory)
            montage.set_editor_property('enable_auto_blend_out', False)
            for prop in ('blend_in', 'blend_out'):
                blend = montage.get_editor_property(prop)
                blend.set_editor_property('blend_time', .22)
                blend.set_editor_property('blend_option', unreal.AlphaBlendOption.CUBIC)
                montage.set_editor_property(prop, blend)
            assert unreal.EditorAssetLibrary.save_loaded_asset(montage)
        assert not montage.get_editor_property('enable_auto_blend_out'), montage.get_path_name()
        track = montage.get_editor_property('slot_anim_tracks')[0]
        assert str(track.get_editor_property('slot_name')) == 'DefaultSlot'
        segment = track.get_editor_property('anim_track').get_editor_property('anim_segments')[0]
        assert segment.get_editor_property('anim_reference') == sequence, (model['id'], state)
        receipt.append(dict(model=model['id'], state=state, montage=montage.get_path_name(),
                            sequence=source, created=created, holdLastPose=True))
(OUT / 'montage-coverage.json').write_text(json.dumps(receipt, indent=2), encoding='utf8')
unreal.log('OPUS_3C_ASSETS_PASS models=%d montages=%d created=%d' % (len(models), len(receipt), sum(x['created'] for x in receipt)))

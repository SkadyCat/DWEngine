"""Read back saved native motion references without modifying the WBP or DataAssets."""
import json
from pathlib import Path
import unreal

output = Path(unreal.Paths.project_saved_dir()) / 'SvgCanvas' / 'motion-assets-reload.json'
output.parent.mkdir(parents=True, exist_ok=True)
try:
    document = unreal.load_asset('/Game/UICreator/DA_CurrentCanvas')
    blueprint = unreal.load_asset('/Game/UICreator/WBP_CurrentCanvas')
    source = json.loads(document.get_editor_property('project_json'))
    expected = source.get('motions', {})
    nodes = [node for node in unreal.ObjectIterator(unreal.SvgCanvasMotionNode)
             if node.get_path_name().startswith('/Game/UICreator/WBP_CurrentCanvas.')]
    references = {}
    for node in nodes:
        asset = node.get_editor_property('motion_asset')
        properties = json.loads(node.get_editor_property('source_properties_json'))
        reference = properties.get('motion', {})
        asset_id = reference.get('assetId', '') if reference.get('preset') != 'none' else ''
        if asset_id:
            assert asset, f'Missing native motion on {node.get_editor_property("source_id")}'
            assert asset.get_editor_property('source_asset_id') == asset_id
            assert json.loads(asset.get_editor_property('definition_json')) == expected[asset_id]
            references[asset_id] = references.get(asset_id, 0) + 1
        else:
            assert not asset, 'Unexpected motion reference'
    assert nodes and references and sum(references.values()) > len(references), 'No reusable motion references found'
    receipt = {'ok': True, 'host': 'fresh editor Python asset reload',
               'sourceHash': document.get_editor_property('source_hash'),
               'nativeMotionNodes': len(nodes), 'sharedReferences': references,
               'definitionsEqualToWebDocument': True}
except Exception as error:
    receipt = {'ok': False, 'error': str(error)}
output.write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log(json.dumps(receipt, ensure_ascii=False))
if not receipt['ok']:
    raise RuntimeError(receipt['error'])

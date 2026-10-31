"""Validate the UICreator handoff, including preservation of existing loot UI."""
import json
import hashlib
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
contract = json.loads((root / 'Content/UICreatorWorld/contract.json').read_text(encoding='utf-8'))
source = contract['project']
baseline = json.loads((root / 'Tools/Fixtures/EntityOverlayBaseline.json').read_text(encoding='utf-8'))
def digest(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
templates = {node['id']: node for node in source['root']['children']}
for key, expected in baseline['templates'].items():
    current = templates[key]
    if key == 'monster':
        current = {**current, 'children': [node for node in current['children'] if node['id'] not in ('monster-status-base', 'monster-status-shield')]}
    assert digest(current) == expected, f'Existing template changed: {key}'
for key, expected in baseline['assets'].items():
    assert digest(source['assets'][key]) == expected, f'Existing SVG asset changed: {key}'
statuses = dict(chill='缓', freeze='冻', shock='电', ignite='燃', bleed='血', slow='慢', poison='毒', erosion='蚀', stun='晕', taunt='嘲')
for status, glyph in statuses.items():
    nodes = {node['binding']: node for node in templates['status-' + status]['children'] if node.get('binding')}
    assert nodes['world-status-glyph']['text'] == glyph
    assert nodes['world-status-seconds']['text'] == '' and nodes['world-status-stacks']['text'] == '', 'Preview values leaked into the runtime source'
    assert nodes['world-status-step']['layout']['width'] == 21
    assert all(nodes[key].get('source', {}).get('overflow') == 'shrink' for key in ('world-status-glyph', 'world-status-seconds', 'world-status-stacks'))
for key, raster in contract['rasters'].items():
    data = (root / 'Content/UICreatorWorld/rasters' / (key + '.png')).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', data[16:24]) == (raster['width'], raster['height']), f'Raster dimension mismatch: {key}'
assert list(contract['states']) == ['canvas'], 'Unexpected HUD or gameplay state in the world overlay export'
hero = {node['id']: node for node in templates['hero']['children']}
native = {node['id']: node for node in contract['states']['canvas']['nodes']}
node = hero['hero-stun-fill']
assert node['binding'] == 'world-stun' and node['interactable'] is False
assert node['layout']['width'] == 68 and node['layout']['height'] == 8
assert node['slot']['offsets']['left'] == node['slot']['offsets']['top'] == 14
assert native[node['id']]['raster'] in contract['rasters'], 'Missing yellow overlay raster'
asset = source['assets'][node['svgAsset']]['source']
assert '#f7d046' in asset and 'fill-opacity="0.55"' in asset
assert not any('hero-stun-slab-' in key or 'hero-stun-chip-' in key for key in hero | native), 'Obsolete earth artwork'
assert 'hero-stun-break' not in hero and 'hero-stun-break' not in native, 'Stale combined debris node'
assert 'entity-svg-hero-stun-break' not in source['assets'], 'Stale debris asset'
print(json.dumps(dict(sourceHash=contract['sourceHash'], nodes=len(contract['nodes']), rasters=len(contract['rasters']), dimensionMismatches=0, preservedTemplates=len(baseline['templates']), preservedAssets=len(baseline['assets']), statuses=len(statuses), yellowStunOverlays=1, previewValuesInExport=0)))

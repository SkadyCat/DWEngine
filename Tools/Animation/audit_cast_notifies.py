"""Read the saved cast sequence/montage and their authored release/end markers."""
from pathlib import Path
import json
import unreal as u

root = Path('G:/DungeonWorldOpus')
output = root / 'Saved/Verification/AttackEnd'
output.mkdir(parents=True, exist_ok=True)
rows = []
for path in ('/Game/Clean3C/Animation/Mage/Set/Set_Mage_Fireball_L_RM',
             '/Game/Clean3C/Animation/Montages/AM_Set_Mage_Fireball_L_RM'):
    asset = u.load_asset(path)
    if not asset:
        raise RuntimeError('Missing ' + path)
    row = {'asset': path, 'length': u.AnimationLibrary.get_sequence_length(asset), 'notifies': []}
    for event in u.AnimationLibrary.get_animation_notify_events(asset):
        notify = event.get_editor_property('notify')
        entry = {'name': str(event.get_editor_property('notify_name')),
                 'class': notify.get_class().get_path_name() if notify else None,
                 'time': u.AnimationLibrary.get_anim_notify_event_trigger_time(event)}
        for prop in ('display_time', 'trigger_time_offset', 'track_index', 'notify_trigger_chance', 'montage_tick_type'):
            try:
                entry[prop] = str(event.get_editor_property(prop))
            except Exception:
                pass
        row['notifies'].append(entry)
    rows.append(row)
(output / 'saved-markers.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
u.log('CAST_MARKER_AUDIT ' + json.dumps(rows))

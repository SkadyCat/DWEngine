"""Calibrate visual equipment from authored grips and generate the AS catalogue."""
import json
import pathlib
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODELS = {m['id']: m for m in json.loads((ROOT / 'Config/models.json').read_text('utf8'))}
POSES = {}


def pose(model):
    if model not in POSES:
        data = MODELS[model]
        clip = unreal.load_asset(data['clips']['idle'])
        opts = unreal.AnimPoseEvaluationOptions()
        opts.set_editor_property('optional_skeletal_mesh', unreal.load_asset(data['mesh']))
        opts.set_editor_property('extract_root_motion', True)
        opts.set_editor_property('incorporate_root_motion_into_pose', False)
        POSES[model] = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, clip.get_play_length() * .25, opts)
    return POSES[model]


def bone(model, name):
    return unreal.AnimPoseExtensions.get_bone_pose(pose(model), name, unreal.AnimPoseSpaces.WORLD)


def socket(model, name):
    names = {str(n) for n in unreal.AnimPoseExtensions.get_socket_names(pose(model))}
    assert name in names, ('Missing authored equipment socket', model, name, names)
    return unreal.AnimPoseExtensions.get_socket_pose(pose(model), name, unreal.AnimPoseSpaces.WORLD)


def palm(model, hand):
    side = hand[-1]
    wrist = bone(model, hand).translation
    middle = bone(model, 'middle_01_' + side).translation
    index = bone(model, 'index_01_' + side).translation
    forward = middle - wrist
    across = index - middle
    rot = unreal.MathLibrary.make_rot_from_xy(forward, across)
    return unreal.Transform(location=(wrist + middle) * .5, rotation=rot, scale=unreal.Vector(1, 1, 1))


def xyz(v):
    return [round(v.x, 7), round(v.y, 7), round(v.z, 7)]


def encode(t):
    r = t.rotation.rotator()
    return {'location': xyz(t.translation), 'rotation': [r.pitch, r.yaw, r.roll], 'scale': xyz(t.scale3d)}


def inverse_point(t, point):
    return unreal.MathLibrary.inverse_transform_location(t, point)


BASE = '/Game/Skeleton_Collection/'
ASSETS = {'sword': BASE + 'Skeleton_swordsman/Meshes/SM_Sword',
          'axe': BASE + 'Skeleton_with_axe/Meshes/SM_Axe',
          'katana': BASE + 'Skeleton_Samurai/Meshes/SM_Katana',
          'spear': BASE + 'Skeleton_Spearman/Meshes/Spear',
          'shield': BASE + 'Skeleton_Spearman/Meshes/Shield',
          'bow': BASE + 'Skeleton_Archer/Meshes/SeparateMesh/SK_Bow',
          'wand': '/Game/Melee/Wand/SM_CoalLamp_Wand'}
NATIVE = {'samurai': [('katana', 'weaponSocket', 'hand_r')],
          'spearman': [('spear', 'weaponSocket', 'hand_r'), ('shield', 'weapon2Socket', 'hand_l')],
          'swordsman': [('sword', 'weaponSocket', 'hand_r')],
          'axeman': [('axe', 'weaponSocket', 'hand_r')]}
CUSTOM = {'cloth_grunt': [('sword', 'hand_r', .7)], 'rust_warrior': [('sword', 'hand_r', .85)],
          'goblin_archer': [('bow', 'hand_l', .65)],
          'goblin_dagger_scout': [('sword', 'hand_r', .35)],
          'goblin_hammer_chief': [('axe', 'hand_r', .75)],
          'goblin_laborer': [('axe', 'hand_r', .45)],
          'goblin_shaman': [('wand', 'hand_r', .9)],
          'goblin_shield_trooper': [('sword', 'hand_r', .5), ('shield', 'hand_l', .6)]}
BUILT_IN = {'archer': 'bow', 'crossbowman': 'crossbow', 'mage': 'book'}

rows = []
for model in MODELS:
    row = {'id': model, 'builtIn': BUILT_IN.get(model, ''), 'parts': []}
    for kind, attach, hand in NATIVE.get(model, []):
        authored = socket(model, attach)
        grip = palm(model, hand)
        # Independent authored weapon tracks drift/drop away from the hand in
        # several attack/death clips. Preserve their grip, anchor to the hand.
        mount = unreal.MathLibrary.make_relative_transform(authored, bone(model, hand))
        part = {'kind': kind, 'asset': ASSETS[kind], 'skeletal': False, 'socket': hand,
                'sourceSocket': attach, 'hand': hand, 'transform': encode(mount),
                'assetGrip': xyz(inverse_point(authored, grip.translation)),
                'handGrip': xyz(inverse_point(bone(model, hand), grip.translation)), 'nativeSocket': False}
        row['parts'].append(part)
    for kind, hand, scale in CUSTOM.get(model, []):
        source_model = 'archer' if kind == 'bow' else 'spearman' if kind == 'shield' else 'axeman' if kind == 'axe' else 'swordsman'
        source_hand = 'hand_l' if kind in ('bow', 'shield') else 'hand_r'
        authored = unreal.Transform() if kind == 'bow' else socket(source_model, 'weapon2Socket' if kind == 'shield' else 'weaponSocket')
        if kind == 'wand':
            turn = unreal.Transform(rotation=unreal.MathLibrary.make_rot_from_z(unreal.Vector(0, 1, 0)))
            authored = unreal.MathLibrary.compose_transforms(turn, authored)
        source_palm = palm(source_model, source_hand)
        target_palm = palm(model, hand)
        relative = unreal.MathLibrary.make_relative_transform(authored, source_palm)
        relative.translation = relative.translation * scale
        relative.scale3d = unreal.Vector(scale, scale, scale)
        palm_to_hand = unreal.MathLibrary.make_relative_transform(target_palm, bone(model, hand))
        mount = unreal.MathLibrary.compose_transforms(relative, palm_to_hand)
        part = {'kind': kind, 'asset': ASSETS[kind], 'skeletal': kind == 'bow', 'socket': hand, 'hand': hand,
                'transform': encode(mount), 'assetGrip': xyz(inverse_point(authored, source_palm.translation)),
                'handGrip': xyz(inverse_point(bone(model, hand), target_palm.translation)), 'nativeSocket': False,
                'calibrationSource': source_model}
        if kind == 'bow':
            part['animation'] = MODELS['archer']['clips']['idle']
            part['freezeTime'] = unreal.load_asset(part['animation']).get_play_length() * .25
        row['parts'].append(part)
    assert row['builtIn'] or row['parts'], ('Missing loadout', model)
    # Fit long authored blades to each creature's idle height, scaling around
    # the measured grip so the weapon does not lose contact with the palm.
    for part in row['parts']:
        if part['skeletal']:
            continue
        t = part['transform']
        mount = unreal.Transform(location=unreal.Vector(*t['location']), rotation=unreal.Rotator(pitch=t['rotation'][0],yaw=t['rotation'][1],roll=t['rotation'][2]), scale=unreal.Vector(*t['scale']))
        box = unreal.load_asset(part['asset']).get_bounding_box()
        def lowest(value):
            world = unreal.MathLibrary.compose_transforms(value, bone(model, part['hand']))
            return min(unreal.MathLibrary.transform_location(world, unreal.Vector(x,y,z)).z
                       for x in (box.min.x,box.max.x) for y in (box.min.y,box.max.y) for z in (box.min.z,box.max.z))
        low = lowest(mount)
        if low < 2:
            grip = unreal.Vector(*part['handGrip'])
            grip_z = unreal.MathLibrary.transform_location(bone(model, part['hand']), grip).z
            factor = (grip_z-2)/(grip_z-low)
            assert 0 < factor <= 1, (model, part['kind'], factor)
            mount.translation = grip+(mount.translation-grip)*factor
            mount.scale3d = mount.scale3d*factor
            part['idleFitScale'] = factor
            part['transform'] = encode(mount)
        part['idleBoundsMinZ'] = lowest(mount)
    rows.append(row)

(ROOT / 'Config/equipment.json').write_text(json.dumps(rows, indent=2), encoding='utf8')


def v(values):
    return 'FVector(' + ','.join(format(n, '.9g') for n in values) + ')'


lines = ['// Generated by Tools/prepare_equipment.py from measured authored grips.',
         '// Visuals only. Source meshes, skeletons, animations and combat rules are unchanged.',
         'struct FC3Gear', '{', ' FString Kind,Asset,Animation;', ' FName Socket,Hand;',
         ' FVector Offset,Scale,AssetGrip,HandGrip;', ' FRotator Rotation;',
         ' double FreezeTime=0;', ' bool Skeletal=false;', '}', 'namespace C3Equipment', '{',
         ' int BuiltInCount(FString Id){return Id=="archer"||Id=="crossbowman"||Id=="mage"?1:0;}',
         ' TArray<FC3Gear> ForModel(FString Id)', ' {', '  TArray<FC3Gear> Out;']
for row in rows:
    if not row['parts']:
        continue
    lines.append('  if(Id=="' + row['id'] + '")')
    lines.append('  {')
    for p in row['parts']:
        t = p['transform']
        lines += ['   {FC3Gear G;G.Kind="%s";G.Asset="%s";G.Socket=n"%s";G.Hand=n"%s";' % (p['kind'], p['asset'], p['socket'], p['hand']),
                  '    G.Offset=%s;G.Scale=%s;G.Rotation=FRotator(%s);' % (v(t['location']), v(t['scale']), ','.join(format(n, '.9g') for n in t['rotation'])),
                  '    G.AssetGrip=%s;G.HandGrip=%s;' % (v(p['assetGrip']), v(p['handGrip']))]
        if p['skeletal']:
            lines.append('    G.Skeletal=true;G.Animation="%s";G.FreezeTime=%.9g;' % (p['animation'], p['freezeTime']))
        lines.append('    Out.Add(G);}')
    lines += ['   return Out;', '  }']
lines += ['  return Out;', ' }', '}', '']
(ROOT / 'Script/Equipment.as').write_text('\n'.join(lines), encoding='utf8')
unreal.log('C3_EQUIPMENT_PREPARE_PASS models=%d externalParts=%d builtIn=%d' % (len(rows), sum(len(r['parts']) for r in rows), len(BUILT_IN)))

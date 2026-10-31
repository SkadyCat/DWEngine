"""Inspect the selected vendor system without modifying it."""
from pathlib import Path
import json
import unreal as u

root = Path('G:/DungeonWorldOpus')
system = u.load_asset('/Game/_TornadoWindVFX/NS/NS_TopDown_Whirlwind')
component = u.new_object(u.NiagaraComponent)
component.set_asset(system)
values = {}
for p in u.NiagaraFunctionLibrary.get_all_user_parameters(system):
    name = str(p.parameter_name)
    kind = str(p.type_name)
    getter = {'NiagaraFloat': 'get_variable_float', 'NiagaraBool': 'get_variable_bool',
              'LinearColor': 'get_variable_color'}.get(kind)
    try:
        result = getattr(component, getter)('User.' + name)
        values[name] = {'type': kind, 'value': str(result)}
    except Exception as exc:
        values[name] = {'type': kind, 'error': str(exc)}
mesh = u.load_asset('/Game/_TornadoWindVFX/_GenericSource/FBX/SM_WindSphere')
report = {'parameters': values, 'mesh_bounds': str(mesh.get_bounds())}
(root / 'Saved/Verification/WindVFX/defaults.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('WIND_DEFAULTS_PASS ' + json.dumps(report))

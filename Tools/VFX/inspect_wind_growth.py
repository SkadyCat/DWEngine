"""Read Niagara emitter graphs/curves without modifying assets."""
from pathlib import Path
import json
import unreal as u

out = Path('G:/DungeonWorldOpus/Saved/Verification/WhirlwindParity/DungeonGround')
out.mkdir(parents=True, exist_ok=True)
system = u.load_asset('/Game/_TornadoWindVFX/NS/NS_TopDown_Whirlwind')
task = u.AssetExportTask()
task.object = system
task.filename = str(out / 'wind-source.t3d')
task.exporter = u.ObjectExporterT3D()
task.automated = True
task.prompt = False
task.replace_identical = True
u.Exporter.run_asset_export_task(task)

def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None

def inspect(obj, names):
    return {name: str(prop(obj, name)) for name in names if prop(obj, name) is not None}

report = {'system': inspect(system, ['warmup_time', 'warmup_tick_delta', 'warmup_tick_count']), 'emitters': []}
report['objects'] = []
for obj in u.ObjectIterator():
    if isinstance(obj, type) or not u.SystemLibrary.get_path_name(obj).startswith(system.get_path_name() + ':'):
        continue
    cls = obj.get_class().get_name()
    if not any(word in cls for word in ['Curve', 'NodeInput', 'FunctionCall', 'Emitter']):
        continue
    row = {'path': obj.get_path_name(), 'class': cls}
    row.update(inspect(obj, ['function_name', 'function_script', 'input', 'data_interface']))
    variable = prop(obj, 'input')
    if variable:
        row['variable'] = inspect(variable, ['name', 'type_def'])
    for name in ['curve', 'x_curve', 'y_curve', 'z_curve']:
        curve = prop(obj, name)
        if curve is not None:
            row[name] = [inspect(key, ['time', 'value', 'interp_mode', 'arrive_tangent', 'leave_tangent']) for key in (prop(curve, 'keys') or [])]
    report['objects'].append(row)
for handle in prop(system, 'emitter_handles') or []:
    item = {'handle': inspect(handle, ['name', 'id', 'versioned_instance', 'instance'])}
    versioned = prop(handle, 'versioned_instance')
    emitter = prop(versioned, 'emitter') if versioned else prop(handle, 'instance')
    item['emitter'] = str(emitter)
    item['versions'] = []
    for data in (prop(emitter, 'version_data') or []):
        version = inspect(data, ['version', 'sim_target', 'graph_source'])
        source = prop(data, 'graph_source')
        graph = prop(source, 'node_graph')
        version['nodes'] = []
        for node in prop(graph, 'nodes') or []:
            row = {'object': node.get_path_name(), 'class': node.get_class().get_name()}
            row.update(inspect(node, ['function_name', 'function_script', 'override_pins', 'input', 'data_interface']))
            di = prop(node, 'data_interface')
            if di:
                row['curve'] = inspect(di, ['curve', 'x_curve', 'y_curve', 'z_curve', 'float_curves', 'color_curve'])
            version['nodes'].append(row)
        item['versions'].append(version)
    report['emitters'].append(item)
(out / 'niagara-growth-audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('WIND_GROWTH_AUDIT_DONE emitters=' + str(len(report['emitters'])))

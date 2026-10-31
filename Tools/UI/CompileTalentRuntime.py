"""Compile a compact loading projection from the immutable UICreator contract."""
import copy,hashlib,json,sys
from pathlib import Path
NODE_FIELDS=('id','parentId','type','name','binding','text','style','source','action')
STYLE_FIELDS=('fill','radius','fontFamily','fontSize','fontWeight','color','letterSpacing','wrap','textAlign','lineHeight','textPadding','textPaddingX','textPaddingY','clip','rowGap','opacity')
SOURCE_FIELDS=('runtimeRasterFrom','input','placeholder','options','htmlId','computed','tag','classes','nodeColor','graphScale','origin','vectorPaths','vectorPolygons')
LAYOUT_FIELDS=('id','x','y','width','height','raster','brushRasters','paintOrder','localVisibility')
def compile_runtime(doc,directory='UICreatorTalents'):
 result={k:copy.deepcopy(doc[k]) for k in ('schema','version','sourceHash','viewport','rasters')}
 result['runtimeProjection']=2;result['vectorTemplates']={};result['nodes']=[]
 for key,value in result['rasters'].items():
  value.pop('png',None);value.pop('svg',None)
 for original in doc['nodes']:
  node={k:copy.deepcopy(original[k]) for k in NODE_FIELDS if k in original}
  node['style']={k:v for k,v in node.get('style',{}).items() if k in STYLE_FIELDS}
  source={k:copy.deepcopy(v) for k,v in original.get('source',{}).items() if k in SOURCE_FIELDS}
  if 'computed' in source: source['computed']={k:v for k,v in source['computed'].items() if k=='whiteSpace'}
  states=original.get('source',{}).get('circleStates')
  if states:
   states=copy.deepcopy(states)
   # Six decimal normalized coordinates differ by less than .0001 screen px at 1400%.
   for state in states:
    for circle in state['circles']:
     for k in ('cx','cy','r'):circle[k]=round(circle[k],6)
   key=hashlib.sha256(json.dumps(states,sort_keys=True,separators=(',',':')).encode()).hexdigest()[:20]
   result['vectorTemplates'][key]={'circleStates':states};source['circleTemplate']=directory+':'+key
  if states or 'vectorPaths' in source or 'vectorPolygons' in source:source['runtimeVector']=True
  for p in source.get('vectorPaths',[]):p.pop('sourcePath',None)
  node['source']=source;result['nodes'].append(node)
 def layout(value):return {k:copy.deepcopy(v) for k,v in value.items() if k in LAYOUT_FIELDS}
 result['states']={k:{'nodes':[layout(n) for n in v['nodes']]} for k,v in doc['states'].items()}
 result['authoringNodes']=[layout(n) for n in doc['authoringNodes']]
 return result
if __name__=='__main__':
 source=Path(sys.argv[1]);target=Path(sys.argv[2]);doc=json.loads(source.read_text(encoding='utf-8-sig'));runtime=compile_runtime(doc);target.write_text(json.dumps(runtime,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
 print(json.dumps({'bytes':target.stat().st_size,'nodes':len(runtime['nodes']),'sharedCircleTemplates':len(runtime['vectorTemplates']),'sourceHash':runtime['sourceHash']}))

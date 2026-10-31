"""Small authoring-tree edit shared by the saved WBP patch and UICreator document."""
from copy import deepcopy

def add_stun_row(project):
 prefab=project['prefabs']['rs-prefab-resources']
 root=prefab['root'];nodes={n['id']:n for n in root['children']}
 if 'rs-stun' in nodes:return False
 for suffix,binding,text in (('title','stun-title','晕眩'),('value','stun-text','80 / 80'),('','stun','')):
  source='rs-mana'+('-'+suffix if suffix else '')
  n=deepcopy(nodes[source]);n['id']='rs-stun'+('-'+suffix if suffix else '')
  n['name']=text or 'stun';n['text']=text;n['binding']=binding;n['slot']['offsets']['top']+=34
  n['style']['color']='#b29665' if not suffix else '#e9b75c';n['value']=1 if not suffix else n.get('value',0)
  root['children'].append(n)
 for n in (root,nodes['rs-resources-bg']):
  n['slot']['offsets']['bottom']+=34;n['layout']['height']+=34
 prefab['height']+=34
 def walk(v):
  if isinstance(v,dict):
   if v.get('id')=='rs-resources-instance':
    v['slot']['offsets']['bottom']+=34;v['layout']['height']+=34
   for k,x in v.items():
    if k not in ('assets','svg','svgAssets'):walk(x)
  elif isinstance(v,list):
   for x in v:walk(x)
 walk(project)
 return True

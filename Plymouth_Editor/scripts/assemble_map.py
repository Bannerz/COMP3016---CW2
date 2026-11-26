"""Build a temporary detailed scene from the saved light layout and saved templates.
Never saves over authoring files. Run with Blender --background Map_Layout.blend.
"""
import bpy,os,sys,json,argparse,hashlib,runpy,collections
from mathutils import Matrix,Vector,Euler
HERE=os.path.dirname(os.path.abspath(__file__));sys.path.insert(0,HERE)
from editor_common import *
from export_progress import report_progress
parser=argparse.ArgumentParser();parser.add_argument('--output',required=True);parser.add_argument('--inspect-only',action='store_true');parser.add_argument('--physics-only',action='store_true');parser.add_argument('--catalog')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
ROOT=os.path.dirname(HERE);output=os.path.abspath(args.output);os.makedirs(output,exist_ok=True)
catalog_path=os.path.abspath(args.catalog or os.path.join(ROOT,'catalog.json'));catalog=json.load(open(catalog_path));base=os.path.dirname(catalog_path)
report_progress(3, 'Reading layout and placements')
layout=bpy.context.scene
assert layout.get('editor_schema')==1,'Open Map_Layout.blend, not the legacy detailed scene'
terrain=next(o for o in layout.objects if o.get('editor_terrain'))
n=int(terrain['editor_grid_size']);assert len(terrain.data.vertices)==n*n,'Terrain topology changed: undo subdivision/deletion; edit heights only'
assert terrain.data.attributes.get('BaselineHeight'),'Terrain baseline attribute is missing'
assert all(abs(v.co.x-(float(terrain['editor_min'])+(i%n)*float(terrain['editor_step'])))<.005 and abs(v.co.y-(float(terrain.get('editor_min_y',terrain['editor_min']))+(i//n)*float(terrain['editor_step'])))<.005 for i,v in enumerate(terrain.data.vertices)),'Terrain XY grid changed: move terrain vertices along Z only'
assert terrain.matrix_world==Matrix.Identity(4),'Keep the terrain object transform unchanged; edit heights in Edit/Sculpt Mode'
delta_active=any(abs(v.co.z-terrain.data.attributes['BaselineHeight'].data[i].value)>1e-5 for i,v in enumerate(terrain.data.vertices))
def delta(x,y):return terrain_height(terrain,x,y)-terrain_height(terrain,x,y,True) if delta_active else 0.
records=[];seen_ids=set();seen_names=set()
# Blender may enumerate a newly linked duplicate before the original marker.
# Reserve original IDs/names before iteration so copies cannot steal its doors.
marker_groups=collections.defaultdict(list)
for ob in layout.objects:
 if ob.get('template_id') and not ob.get('placement_cloud') and not ob.get('editor_preview') and ob.get('editor_enabled',True):marker_groups[ob.get('placement_id',identity(ob.name))].append(ob)
marker_keepers={token:sorted(group,key=lambda o:(o.name!='Place | '+o.get('source_name',''),o.name))[0] for token,group in marker_groups.items()}
def record(key,token,name,matrix,values):
 assert key in catalog['assets'],'Unknown template '+key
 suffix=0;original=token
 while token in seen_ids:suffix+=1;token=identity(original+'|copy|'+str(suffix))
 seen_ids.add(token)
 if name in seen_names:name=name+' | copy '+token
 seen_names.add(name)
 matrix=matrix.copy();p=matrix.translation;matrix.translation.z+=delta(p.x,p.y)
 records.append({'template':key,'id':token,'name':name,'matrix':matrix,'props':values})
for ob in layout.objects:
 if not ob.get('editor_enabled',True) or ob.get('editor_preview'):continue
 key=ob.get('template_id')
 if not key:continue
 if ob.get('placement_cloud'):
  mesh=ob.data;stored=json.loads(ob['placement_records']);attrs=mesh.attributes
  assert all(attrs.get(k) for k in ['PlacementId','PlacementScale','PlacementRotation']),'Missing point attributes on '+ob.name
  for i,v in enumerate(mesh.vertices):
   index=attrs['PlacementId'].data[i].value;old=stored[index-1] if 0<index<=len(stored) else {}
   scale=Vector(attrs['PlacementScale'].data[i].vector);rotation=Euler(attrs['PlacementRotation'].data[i].vector)
   assert min(scale)>0,'Point scale must be positive: '+ob.name
   m=ob.matrix_world@Matrix.LocRotScale(v.co,rotation.to_quaternion(),scale)
   record(key,old.get('id',identity(ob.name+'|'+str(i))),old.get('source_name',key+' | '+str(i)),m,old.get('props',{}))
 else:
  token=ob.get('placement_id',identity(ob.name));name=ob.get('source_name',ob.name)
  if marker_keepers[token]!=ob:token=identity(token+'|marker|'+ob.name);name=name+' | copy '+token
  record(key,token,name,ob.matrix_world,json.loads(ob.get('placement_props','{}')))
by_id={r['id']:r for r in records};ground={o['editor_environment_id']:o for o in layout.objects if o.get('editor_geometry') and o.get('editor_enabled',True)}
markers=[]
for ob in layout.objects:
 if ob.get('game_marker_type') and ob.get('editor_enabled',True):
  m=ob.matrix_world.copy();p=m.translation;m.translation.z+=delta(p.x,p.y);markers.append((ob.name,m,props(ob)))
world=bpy.data.scenes.new('PLYMOUTH | assembled for export');bpy.context.window.scene=world
def collection(name):
 c=world.collection.children.get(name)
 if not c:c=bpy.data.collections.new(name);world.collection.children.link(c)
 return c
def link(ob,col):col.objects.link(ob);return ob
def mesh_from(obj,graph):
 if obj.modifiers or obj.type!='MESH':return bpy.data.meshes.new_from_object(obj.evaluated_get(graph),preserve_all_data_layers=True,depsgraph=graph)
 return obj.data
def deform(ob):
 if not delta_active or ob.type!='MESH':return
 ob.data=ob.data.copy();inv=ob.matrix_world.inverted()
 for v in ob.data.vertices:
  p=ob.matrix_world@v.co;p.z+=delta(p.x,p.y);v.co=inv@p
def append_scene(path,asset_key=None):
 with bpy.data.libraries.load(path,link=False) as (src,dst):
  assert src.scenes,'No scene found in '+path
  dst.scenes=list(src.scenes)
 if asset_key:
  matches=[s for s in dst.scenes if any(o.get('editor_asset_key')==asset_key for o in s.objects)]
  assert matches,'ASSET_ROOT is missing in '+path
  return matches[0]
 return next((s for s in dst.scenes if any(o.get('editor_source_name') for o in s.objects)),dst.scenes[0])
report_progress(8, 'Loading the detailed environment')
environment=append_scene(os.path.join(base,catalog['environment_file']))
for source in environment.objects:
 name=source.get('editor_source_name',source.name);support=source.get('editor_support_owner');original_matrix=source.matrix_world.copy()
 if name==TERRAIN_NAME:
  ob=source.copy();ob.data=source.data.copy();ob.parent=None;ob.matrix_world=original_matrix;ob.name=TERRAIN_NAME;link(ob,collection('Terrain'));deform(ob);fine_terrain=ob;continue
 if support:
  if support not in by_id:continue
  ob=source.copy();ob.parent=None;ob.matrix_world=by_id[support]['matrix']@Matrix(json.loads(source['editor_support_baseline'])).inverted()@original_matrix
 else:
  preview=ground.get(source.get('editor_environment_id'))
  if not preview:continue
  ob=preview.copy();ob.data=preview.data.copy();ob.parent=None;ob.matrix_world=preview.matrix_world.copy()
  setprops(ob,props(source))
  if hasattr(ob.data,'materials'):
   ob.data.materials.clear()
   for m in source.data.materials:ob.data.materials.append(m)
 ob.name=name;cols=json.loads(source.get('editor_source_collections','["Environment"]'));link(ob,collection(cols[0] if cols else 'Environment'))
 # Foundations already inherit the placement's height adjustment.
 if not support and not any(s in name.lower() for s in ['water |','ebb water']):deform(ob)
templates={};template_report={}
template_keys = sorted({record['template'] for record in records})
for template_number, key in enumerate(template_keys, 1):
 report_progress(12 + 24 * (template_number - 1) / len(template_keys),
                 f'Loading template {template_number}/{len(template_keys)}')
 entry=catalog['assets'][key];path=os.path.join(base,entry['file']);asset_scene=append_scene(path,key);bpy.context.window.scene=asset_scene;asset_scene.view_layers[0].update();graph=bpy.context.evaluated_depsgraph_get()
 col=next((c for c in asset_scene.collection.children_recursive if c.name==entry['collection'] or c.name.startswith(entry['collection']+'.')),None)
 assert col,'Keep the ASSET collection in '+path
 parts=[];bounds_points=[];root=next((o for o in col.all_objects if o.get('editor_asset_key')==key),None)
 assert root,'ASSET_ROOT is missing in '+path
 for source in template_objects(col):
  if source.hide_render or source.get('editor_exclude'):continue
  mesh=mesh_from(source,graph);evaluated=source.evaluated_get(graph);m=evaluated.matrix_world.copy();values=props(source);bounds_points.extend(m@Vector(p) for p in evaluated.bound_box)
  # Newly added objects work without custom properties; unique names identify parts.
  part=values.get('source_part',values.get('editor_part_id',source.name));values['source_part']=part
  parts.append({'mesh':mesh,'matrix':m,'props':values,'part':part,'door':bool(source.get('interactive_door') or source.get('editor_role')=='door')})
 assert parts,'No visible geometry in '+path
 # Use exact edited geometry for shared static interior collision, including modifiers.
 collision=None
 if entry['kind'] in ['house','church']:
  verts=[];faces=[]
  for part in parts:
   if part['door'] or part['props'].get('editor_collision')=='none':continue
   me=part['mesh'];offset=len(verts);verts.extend([part['matrix']@v.co for v in me.vertices]);faces.extend([tuple(offset+i for i in p.vertices) for p in me.polygons])
  collision=bpy.data.meshes.new('Shared interior | '+key);collision.from_pydata(verts,[],faces)
 bounds=[[min(p[i] for p in bounds_points) for i in range(3)],[max(p[i] for p in bounds_points) for i in range(3)]]
 templates[key]={'entry':entry,'parts':parts,'collision':collision,'root_props':props(root),'path':path,'bounds':bounds}
 template_report[key]={'instances':sum(r['template']==key for r in records),'parts':len(parts),'vertices':sum(len(p['mesh'].vertices) for p in parts),'sha256':hashlib.sha256(open(path,'rb').read()).hexdigest(),'collision_vertices':len(collision.vertices) if collision else 0}
 print('ASSEMBLE_TEMPLATE',key,template_report[key]['instances'],flush=True)
report_progress(36, 'Placing meshes in the map')
bpy.context.window.scene=world;placed=collection('Template placements');owners=[];report_placements=[]
for ob in world.objects:
 if not ob.get('editor_support_owner'):continue
 r=by_id[ob['editor_support_owner']];template=templates[r['template']];old=template['entry']['bounds'];new=template['bounds'];fit=Matrix.Identity(4)
 for axis in [0,1]:
  ratio=(new[1][axis]-new[0][axis])/max(.01,old[1][axis]-old[0][axis]);offset=(new[1][axis]+new[0][axis])/2-ratio*(old[1][axis]+old[0][axis])/2
  if abs(ratio-1)>1e-5 or abs(offset)>1e-5:fit[axis][axis]=ratio;fit[axis][3]=offset
 ob.matrix_world=r['matrix']@fit@r['matrix'].inverted()@ob.matrix_world
for r in records:
 template=templates[r['template']];entry=template['entry'];kind=entry['kind'];m=r['matrix'];name=r['name']
 root=link(bpy.data.objects.new(name+' | placement',None),placed);root.matrix_world=m;setprops(root,r['props']);root['editor_placement_id']=r['id']
 if template['root_props'].get('editor_stair_route'):
  route=json.loads(template['root_props']['editor_stair_route']);root['game_stair_route']=json.dumps([list(m@Vector(p)) for p in route]);root['game_stair_asset']=r['template'];root['game_stair_owner']=name;root['game_stair_width']=float(template['root_props'].get('editor_stair_width',1.))*m.to_scale().x
 # Old per-mesh properties must not turn the placement Empty into a door/landmark twice.
 for k in ['interactive_door','door_id','door_group']:
  if k in root:del root[k]
 body_seen=False;door_count=0
 for part in template['parts']:
  body_name=name if not body_seen and not part['door'] else name+' | '+part['part']
  if not part['door']:body_seen=True
  ob=link(bpy.data.objects.new(body_name,part['mesh']),placed);ob.parent=root;ob.matrix_parent_inverse=Matrix.Identity(4);ob.matrix_basis=part['matrix'];setprops(ob,part['props']);ob['replacement_source']=template['path'];ob['asset_role']=kind
  for k in ['game_landmark_name','game_landmark_kind']:
   if k in ob:del ob[k]
  if kind in ['house','church']:ob['collision_kind']='none'
  if kind=='tree':
   scale=m.to_scale();ob['tree_collision_radius']=float(template['root_props'].get('trunk_radius',.2))*max(abs(scale.x),abs(scale.y));ob['tree_collision_height']=min(5.,max(1.,(entry['bounds'][1][2]-entry['bounds'][0][2])*abs(scale.z)*.6))
  if kind=='detail':ob['collision_kind']='none'
  if part['door']:
   ob['collision_kind']='none';owners.append((ob,name,m.translation.copy(),entry['entry_asset']));door_count+=1
  elif kind=='trade' and ob.get('collision_kind') not in ['none','well']:ob['collision_kind']='triangles'
 if template['collision']:
  ob=link(bpy.data.objects.new('Interior | '+r['id'],template['collision']),placed);ob.matrix_world=m;ob.hide_render=True;ob['collision_kind']='instanced';ob['collision_only']=True
 report_placements.append({'id':r['id'],'name':name,'template':r['template'],'position':list(m.translation),'doors':door_count,'render_parts':len(template['parts'])})
for name,m,values in markers:
 ob=link(bpy.data.objects.new(name,None),collection('Gameplay markers'));ob.matrix_world=m;setprops(ob,values)
world.view_layers[0].update()
def fine_height(x,y):
 u=max(0,min(598.999,(x+1499.5)/5));v=max(0,min(598.999,(1499.5-y)/5));i=int(u);j=int(v);a=u-i;b=v-j;vs=fine_terrain.data.vertices
 return vs[j*600+i].co.z*(1-a)*(1-b)+vs[j*600+i+1].co.z*a*(1-b)+vs[(j+1)*600+i].co.z*(1-a)*b+vs[(j+1)*600+i+1].co.z*a*b
report_progress(46, 'Preparing doors and stonework')
verts,faces,groups=annotate_doors(world,owners,fine_height)
from masonry import palette,clad_foundations,entry_steps
palettes={None:palette(templates['House_Gabled_Artisan_House'])}
for r in records:
 if templates[r['template']]['entry']['kind'] in ['house','church','trade']:
  stones=palette(templates[r['template']]);palettes[r['id']]=stones;palettes[r['name']]=stones
masonry_report={'foundations':clad_foundations(world,palettes,fine_height),'entrances':entry_steps(world,owners,palettes,fine_height)}
report={'schema':1,'layout':bpy.data.filepath,'templates':template_report,'placements':report_placements,'doors':len(owners),'entry_groups':groups,'terrain_deformed':delta_active,'markers':[{'name':name,'position':list(m.translation)} for name,m,v in markers], 'door_positions':[{'id':ob['door_id'],'owner':name,'position':list(ob.matrix_world.translation)} for ob,name,center,asset in owners], 'supports':[{'owner':o['editor_support_owner'],'matrix':matrows(o.matrix_world)} for o in world.objects if o.get('editor_support_owner')]}
report['masonry']=masonry_report
json.dump(report,open(os.path.join(output,'editor_build.json'),'w'),indent=2)
print('ASSEMBLY_COMPLETE',len(records),'placements',len(owners),'doors',groups,'entries',flush=True)
if args.inspect_only:
 report_progress(99, 'Assembly inspection complete')
else:
 os.environ['PLYMOUTH_EXPORT_DIR']=output
 if args.physics_only:os.environ['PLYMOUTH_PHYSICS_ONLY']='1'
 runpy.run_path(os.path.join(HERE,'export_world.py'),run_name='__main__')

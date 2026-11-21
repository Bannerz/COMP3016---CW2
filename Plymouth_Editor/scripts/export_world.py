"""Export the visible Blender world, preserving shared meshes and vertex colours."""
import bpy,os,json,math,struct,numpy as np,random,wave
from mathutils import Vector,Matrix
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS=os.path.abspath(os.environ.get('PLYMOUTH_EXPORT_DIR',os.path.join(ROOT,'assets')));os.makedirs(ASSETS,exist_ok=True)
scene=bpy.context.scene
object_collections={}
for collection in bpy.data.collections:
 for obj in collection.objects:object_collections.setdefault(obj.name,[]).append(collection)
def visible(o):
 return o.type in {'MESH','FONT','CURVE'} and not o.hide_render and not any(c.hide_render for c in object_collections.get(o.name,[]))
objects=[o for o in scene.objects if visible(o)]
print('EXPORT_VISIBLE_OBJECTS',len(objects),flush=True)
terrain=bpy.data.objects['Plymouth inspired | ARTISTIC hills, clefts and streams']
coords=np.empty(len(terrain.data.vertices)*3,np.float32);terrain.data.vertices.foreach_get('co',coords);coords=coords.reshape(600,600,3)
def h(x,y):
 u=max(0,min(598.999,(x+1499.5)/5));v=max(0,min(598.999,(1499.5-y)/5));i=int(u);j=int(v);a=u-i;b=v-j
 z=coords[:,:,2];return float(z[j,i]*(1-a)*(1-b)+z[j,i+1]*a*(1-b)+z[j+1,i]*(1-a)*b+z[j+1,i+1]*a*b)
# Engine coordinates: X east, Y up, Z south. Blender (x,y,z)->(x,z,-y).
with open(os.path.join(ASSETS,'terrain.bin'),'wb') as f:
 f.write(struct.pack('<IIffff',600,600,-1499.5,-1499.5,5.,5.))
 f.write(coords[:,:,2].astype('<f4').tobytes())

# Box colliders for building shells; walls/bridges use their actual collision triangles.
boxes=[];tri_objects=[]
for o in objects:
 cols=[c.name for c in object_collections.get(o.name,[])]
 if o.get('collision_kind')=='none':continue
 if o.get('collision_kind')=='triangles' or o.name.startswith(('Fitted foundation |','Trade foundation |')):
  if o.type=='MESH':tri_objects.append(o)
  continue
 is_tree='tree' in o.name.lower() and ('woodland' in o.name.lower())
 if o.get('tree_collision_radius') is not None:
  p=o.matrix_world.translation;r=float(o['tree_collision_radius']);half=float(o['tree_collision_height'])/2
  boxes.append([float(p.x),float(p.z+half),float(-p.y),r,half,r]);continue
 if is_tree:
  # Cheap trunk collision, rather than cooking every leaf of every tree.
  p=o.matrix_world.translation;s=o.scale;r=float(o.get('trunk_radius',.21))*max(abs(s.x),abs(s.y));half=2.5*abs(s.z)
  boxes.append([float(p.x),float(p.z+half),float(-p.y),float(r),float(half),float(r)])
  continue
 if o.get('collision_kind')=='well':
  p=o.matrix_world.translation;r=float(o.get('collision_radius',1.5));height=float(o.get('collision_height',3.6));boxes.append([float(p.x),float(p.z+height/2),float(-p.y),r,height/2,r]);continue
 if o==terrain or any(s in o.name.lower() for s in ['water |','ebb water','artistic stream','fishery stage pile']):continue
 is_building=o.get('use') or o.get('collision_kind')=='box' or o.name.startswith(('House ','Infill ')) or any(c.startswith('Outlying hamlets') for c in cols)
 is_building=is_building or any(o.name.endswith(s) for s in [' | farmhouse',' | great barn',' | stable',' | byre',' | pigsty'])
 is_building=is_building or o.name.startswith(('St Andrew |','Castle Quadrate','Castle | interpreted','Millbay tide mill'))
 if is_building and o.type=='MESH':
  from mathutils import Vector
  p=[o.matrix_world@Vector(q) for q in o.bound_box];lo=np.min([q[:] for q in p],axis=0);hi=np.max([q[:] for q in p],axis=0)
  # Conservative walls around the massing; roofs are closed, interiors aren't playable yet.
  boxes.append([float((lo[0]+hi[0])/2),float((lo[2]+hi[2])/2),float(-(lo[1]+hi[1])/2),float((hi[0]-lo[0])/2),float((hi[2]-lo[2])/2),float((hi[1]-lo[1])/2)])
 elif any(c in ['ARTISTIC | stone boundary walls and gates','ARTISTIC | streams and bridges'] for c in cols) or 'fence' in o.name.lower() or 'landing' in o.name.lower():
  if o.type=='MESH' and not o.name.endswith('artistic stream'):tri_objects.append(o)
tri_objects.extend(o for o in scene.objects if o.type=='MESH' and o.get('entry_step_collision') and not any(c.hide_render for c in object_collections.get(o.name,[])))
with open(os.path.join(ASSETS,'colliders.bin'),'wb') as f:
 f.write(struct.pack('<I',len(boxes)))
 for box in boxes:f.write(struct.pack('<6f',*box))
 # The source uses world-space meshes except instances, so apply transforms explicitly.
 tris=[]
 for o in tri_objects:
  me=o.data;me.calc_loop_triangles()
  for tr in me.loop_triangles:
   for k in tr.vertices:
    p=o.matrix_world@me.vertices[k].co;tris.extend((p.x,p.z,-p.y))
 f.write(struct.pack('<I',len(tris)//9));f.write(np.array(tris,dtype='<f4').tobytes())

# Interior collision meshes are cooked once per prototype and instanced by PhysX.
# This avoids flattening hundreds of identical houses into millions of triangles.
C=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)));Ci=C.inverted()
interior_groups={}
for o in scene.objects:
 if o.get('collision_kind')=='instanced' and not any(c.hide_render for c in object_collections.get(o.name,[])):
  interior_groups.setdefault(o.data.name,[]).append(o)
interior_stats={'meshes':len(interior_groups),'instances':0,'unique_triangles':0}
with open(os.path.join(ASSETS,'interiors.bin'),'wb') as f:
 f.write(b'PLYI');f.write(struct.pack('<II',1,len(interior_groups)))
 for name,group in interior_groups.items():
  mesh=group[0].data;mesh.calc_loop_triangles()
  vertices=np.array([(v.co.x,v.co.z,-v.co.y) for v in mesh.vertices],dtype='<f4')
  indices=np.array([tuple(t.vertices) for t in mesh.loop_triangles],dtype='<u4')
  f.write(struct.pack('<II',len(vertices),len(indices)));f.write(vertices.tobytes());f.write(indices.tobytes());f.write(struct.pack('<I',len(group)))
  interior_stats['instances']+=len(group);interior_stats['unique_triangles']+=len(indices)
  for ob in group:
   m=C@ob.matrix_world@Ci;position,rotation,scale=m.decompose()
   f.write(struct.pack('<10f',*position,rotation.x,rotation.y,rotation.z,rotation.w,*scale))
import sys
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from door_metadata import build_door_manifest
door_manifest=build_door_manifest(objects)
print('ENTRY_EXPORT',interior_stats,'doors',len(door_manifest),flush=True)

def export_graphics():
 import hashlib
 # Prepare shared game copies of the supplied models. Keep the editable Blender
 # scene's full-detail geometry and textures intact: this export session is never saved.
 # Keep image materials for architecture. Dense vegetation keeps its cheaper vertex colours.
 replacement_groups={}
 for o in objects:
  if o.get('replacement_source') and o.type=='MESH':replacement_groups.setdefault(o.data.name,[]).append(o)
 texture_cache={};runtime_stats=[];image_materials=set();packed_images={}
 def image_pixels(image):
  if image.name not in texture_cache:
   pixels=np.empty(len(image.pixels),np.float32);image.pixels.foreach_get(pixels)
   texture_cache[image.name]=pixels.reshape(image.size[1],image.size[0],4)
  return texture_cache[image.name]
 for name,group in replacement_groups.items():
  source=group[0];role=source.get('asset_role','house');mesh=source.data.copy()
  temp=bpy.data.objects.new('EXPORT_TEMP',mesh);scene.collection.objects.link(temp)
  mesh.calc_loop_triangles();before=len(mesh.loop_triangles)
  target=9000 if role=='tree' else 24000 if role=='house' else 110000
  if before>target:
   modifier=temp.modifiers.new('Runtime triangle budget','DECIMATE');modifier.ratio=target/before
   depsgraph=bpy.context.evaluated_depsgraph_get();evaluated=temp.evaluated_get(depsgraph)
   simplified=bpy.data.meshes.new_from_object(evaluated,preserve_all_data_layers=True,depsgraph=depsgraph)
   temp.modifiers.clear();temp.data=simplified;bpy.data.meshes.remove(mesh);mesh=simplified
  if role not in {'tree','detail'} and mesh.uv_layers.active and any(m and m.use_nodes and any(n.type=='TEX_IMAGE' and n.image for n in m.node_tree.nodes) for m in mesh.materials):
   # Templates carry duplicate copies of the same packed images. Share them by content.
   for material in mesh.materials:
    if not material or not material.use_nodes or material in image_materials:continue
    image_materials.add(material)
    for node in material.node_tree.nodes:
     if node.type!='TEX_IMAGE' or not node.image or not node.image.packed_file:continue
     img=node.image
     key=(img.colorspace_settings.name,hashlib.sha256(img.packed_file.data).digest())
     node.image=packed_images.setdefault(key,img)
   for attr in list(mesh.color_attributes):mesh.color_attributes.remove(attr)
   # glTF addresses the primary authored UV layer as TEXCOORD_0.
   mesh.uv_layers.active.active_render=True
   mesh.calc_loop_triangles()
   runtime_stats.append({'source_mesh':name,'role':role,'instances':len(group),'source_triangles':before,'runtime_triangles':len(mesh.loop_triangles),'image_materials':True})
   for o in group:o.data=mesh
   bpy.data.objects.remove(temp,do_unlink=True)
   continue
  for layer in list(mesh.uv_layers)[1:]:mesh.uv_layers.remove(layer)
  colors=np.ones((len(mesh.loops),4),np.float32)
  uv=np.empty(len(mesh.loops)*2,np.float32)
  if mesh.uv_layers.active:mesh.uv_layers.active.data.foreach_get('uv',uv);uv=uv.reshape(-1,2)
  loop_material=np.empty(len(mesh.loops),np.int32)
  for polygon in mesh.polygons:loop_material[polygon.loop_start:polygon.loop_start+polygon.loop_total]=polygon.material_index
  for index,material in enumerate(mesh.materials):
   if not material:continue
   mask=loop_material==index
   if not np.any(mask):continue
   bs=next((n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None) if material.use_nodes else None
   base=bs.inputs['Base Color'] if bs else None
   tex=next((n.image for n in material.node_tree.nodes if n.type=='TEX_IMAGE' and n.image),None) if base and base.is_linked else None
   if tex and mesh.uv_layers.active:
    pixels=image_pixels(tex);height,width=pixels.shape[:2];sample=uv[mask]%1
    values=pixels[np.minimum((sample[:,1]*height).astype(int),height-1),np.minimum((sample[:,0]*width).astype(int),width-1)].copy()
    if tex.colorspace_settings.name=='sRGB':values[:,:3]=np.where(values[:,:3]<=.04045,values[:,:3]/12.92,((values[:,:3]+.055)/1.055)**2.4)
    colors[mask]=values
   else:colors[mask]=tuple(base.default_value) if base else tuple(material.diffuse_color)
  colors[:,3]=1
  for attr in list(mesh.color_attributes):mesh.color_attributes.remove(attr)
  attribute=mesh.color_attributes.new(name='ImportedSurface',type='BYTE_COLOR',domain='CORNER');attribute.data.foreach_set('color',colors.ravel())
  material=bpy.data.materials.new(('Woodland | supplied ' if role=='tree' else 'Detail foliage | supplied ' if role=='detail' else 'Supplied model | ')+name);material.diffuse_color=(1,1,1,1);material.use_nodes=True
  vertex=material.node_tree.nodes.new('ShaderNodeVertexColor');vertex.layer_name='ImportedSurface';material.node_tree.links.new(vertex.outputs['Color'],material.node_tree.nodes.get('Principled BSDF').inputs['Base Color'])
  mesh.materials.clear();mesh.materials.append(material)
  for polygon in mesh.polygons:polygon.material_index=0
  mesh.calc_loop_triangles();runtime_stats.append({'source_mesh':name,'role':role,'instances':len(group),'source_triangles':before,'runtime_triangles':len(mesh.loop_triangles)})
  for o in group:o.data=mesh
  bpy.data.objects.remove(temp,do_unlink=True)
  print('RUNTIME_MODEL',name,len(group),before,len(mesh.loop_triangles),flush=True)
 if runtime_stats:
  with open(os.path.join(ASSETS,'replacement_runtime_stats.json'),'w') as f:json.dump(runtime_stats,f,indent=2)
 
 # Simplify node materials to a portable glTF vertex-colour + base-colour contract.
 # Work on this in-memory export session, never save over the original .blend.
 water_materials=set()
 for m in bpy.data.materials:
  if m in image_materials:continue
  name=m.name.lower()
  if not m.name.startswith(('Supplied model | ','Woodland | supplied ')) and any(n in name for n in ['water','freshwater']):water_materials.add(m.name)
  color=tuple(m.diffuse_color)
  if m.use_nodes:
   bs=m.node_tree.nodes.get('Principled BSDF')
   if bs and not bs.inputs['Base Color'].is_linked:color=tuple(bs.inputs['Base Color'].default_value)
  m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;n.clear();out=n.new('ShaderNodeOutputMaterial');bs=n.new('ShaderNodeBsdfPrincipled');bs.inputs['Base Color'].default_value=color;bs.inputs['Roughness'].default_value=.85;l.new(bs.outputs['BSDF'],out.inputs['Surface'])
  if m.name.startswith('Turf, earth') or m.name.startswith(('Supplied model | ','Woodland | supplied ','Detail foliage | supplied ')):
   vertex=n.new('ShaderNodeVertexColor');vertex.layer_name='Landscape' if m.name.startswith('Turf, earth') else 'ImportedSurface';l.new(vertex.outputs['Color'],bs.inputs['Base Color']);bs.inputs['Base Color'].default_value=(1,1,1,1)
  if m.name in water_materials:bs.inputs['Base Color'].default_value=(.045,.21,.26,1);bs.inputs['Roughness'].default_value=.2
 
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=terrain
 filepath=os.path.join(ASSETS,'world.glb')
 # UVs are only needed by meshes that retained image materials.
 for o in objects:
  if o.type=='MESH' and not any(m in image_materials for m in o.data.materials):
   for layer in list(o.data.uv_layers):o.data.uv_layers.remove(layer)
 bpy.ops.export_scene.gltf(filepath=filepath,export_format='GLB',use_selection=True,use_active_scene=True,export_yup=True,export_apply=False,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_cameras=False,export_lights=False,export_animations=False)

if not os.environ.get("PLYMOUTH_PHYSICS_ONLY"):export_graphics()

def loc(x,y):return [x,h(x,y)+1.75,-y]
manifest={'title':'Plymouth: A Medieval Wander','artistic':True,'spawn':loc(-560,190),
 'landmarks':[{'name':'Brookfield Farm','position':loc(-560,250),'kind':'farm'}, {'name':'Westbrook Mill','position':loc(-680,700),'kind':'mill'}, {'name':'Brookside Market','position':loc(-640,380),'kind':'market'}, {'name':'Sutton Fisheries','position':loc(295,-465),'kind':'fishery'}, {'name':'Harbour Smithy','position':loc(-280,-135),'kind':'smithy'}, {'name':'St Andrew\u2019s','position':loc(-80,-105),'kind':'church'}, {'name':'Northwood Farm','position':loc(-420,1020),'kind':'farm'}, {'name':'North Road Market','position':loc(-260,900),'kind':'market'}, {'name':'Eastmeadow Market','position':loc(1150,180),'kind':'market'}, {'name':'Eastburn Mill','position':loc(1275,410),'kind':'mill'}, {'name':'North Ridge Windmill','position':loc(200,1110),'kind':'mill'}],
 'quest':[{'name':'Collect a sack of grain','position':loc(-550,218),'kind':'grain'}, {'name':'Deliver grain to Westbrook Mill','position':loc(-680,700),'kind':'mill'}, {'name':'Take the flour to Brookside Market','position':loc(-640,380),'kind':'market'}, {'name':'Collect a repaired tool from the smithy','position':loc(-280,-135),'kind':'smithy'}, {'name':'Deliver the tool to Sutton Fisheries','position':loc(295,-465),'kind':'fishery'}],
 'asset_stats':{'source_objects':len(objects),'box_colliders':len(boxes),'collision_triangles':len(tris)//9},'source_blend':os.path.basename(bpy.data.filepath)}
manifest['trade_places']=[{'name':o['game_landmark_name'],'kind':o['game_landmark_kind'],'asset':o.get('supplied_asset',''),'position':[float(o.matrix_world.translation.x),float(o.matrix_world.translation.z),float(-o.matrix_world.translation.y)]} for o in scene.objects if o.get('game_landmark_name')]
manifest['doors']=door_manifest;manifest['interior_collision']=interior_stats
manifest['stairs']=[{'asset':o['game_stair_asset'],'owner':o['game_stair_owner'],'width':float(o['game_stair_width']),'route':[[float(p[0]),float(p[2]),float(-p[1])] for p in json.loads(o['game_stair_route'])]} for o in scene.objects if o.get('game_stair_route')]
for marker_type,manifest_key in [('quest','quest'),('landmark','landmarks')]:
 markers=sorted([o for o in scene.objects if o.get('game_marker_type')==marker_type and o.get('editor_enabled',True)],key=lambda o:int(o.get('game_marker_index',0)))
 if markers:manifest[manifest_key]=[{'name':o['game_marker_name'],'kind':o['game_marker_kind'],'position':[float(o.matrix_world.translation.x),float(o.matrix_world.translation.z),float(-o.matrix_world.translation.y)]} for o in markers]
for landmark in manifest['landmarks']:landmark['name']=landmark['name'].replace('\u2019', "'")
open(os.path.join(ASSETS,'world.json'),'w').write(json.dumps(manifest,indent=2))
# Small original procedural ambience clips: no third-party media dependencies.
rate=22050;random.seed(7)
for name,seconds in [('water',4),('wind',5),('forge',2),('step',.22),('chime',.6)]:
 count=int(rate*seconds);t=np.arange(count)/rate;noise=np.array([random.uniform(-1,1) for _ in range(count)])
 if name=='water':signal=np.convolve(noise,np.ones(9)/9,'same')*.23*(.8+.2*np.sin(t*4))+np.sin(t*870)*.008
 elif name=='wind':signal=np.convolve(noise,np.ones(110)/110,'same')*.35*(.6+.4*np.sin(t*1.4))
 elif name=='forge':signal=(np.sin(2*np.pi*880*t)+.4*np.sin(2*np.pi*2310*t))*np.exp(-(t%1)*15)*.22
 elif name=='step':signal=noise*np.exp(-t*27)*.3
 else:signal=(np.sin(t*2*np.pi*660)+np.sin(t*2*np.pi*990)*.4)*np.exp(-t*7)*.22
 with wave.open(os.path.join(ASSETS,name+'.wav'),'wb') as f:f.setparams((1,2,rate,count,'NONE','not compressed'));f.writeframes((np.clip(signal,-1,1)*32767).astype('<i2').tobytes())
print('EXPORT_COMPLETE',json.dumps(manifest['asset_stats']))

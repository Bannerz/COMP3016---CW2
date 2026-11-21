"""Shared offline map-editor data contract. No game/runtime dependencies."""
import bpy,json,math,os,re,hashlib
from mathutils import Matrix,Vector
TERRAIN_NAME='Plymouth inspired | ARTISTIC hills, clefts and streams'
def plain(value):
 if hasattr(value,'to_list'):return value.to_list()
 if hasattr(value,'items'):return {k:plain(v) for k,v in value.items()}
 if isinstance(value,(str,int,float,bool)) or value is None:return value
 try:return [plain(v) for v in value]
 except TypeError:return str(value)
def props(ob):return {k:plain(v) for k,v in ob.items() if k!='_RNA_UI'}
def setprops(ob,values):
 for k,v in values.items():
  if v is not None:
   try:ob[k]=v
   except (TypeError,ValueError):ob[k]=json.dumps(v)
def safe(name):return re.sub(r'[^A-Za-z0-9_]+','_',name).strip('_')
def identity(name):return hashlib.sha1(name.encode()).hexdigest()[:16]
def matrows(matrix):return [list(row) for row in matrix]
def relative_matrix(ob):
 # Works on appended, unlinked template data too (matrix_world may be stale).
 return relative_matrix(ob.parent)@ob.matrix_parent_inverse@ob.matrix_basis if ob.parent else ob.matrix_basis.copy()
def template_objects(collection):return [o for o in collection.all_objects if o.type in {'MESH','CURVE','FONT'}]
def box_bounds(objects):
 points=[relative_matrix(o)@Vector(v) for o in objects if o.type=='MESH' for v in o.bound_box]
 if not points:return [[-.5,-.5,0],[.5,.5,1]]
 return [[min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)]]
def terrain_height(terrain,x,y,baseline=False):
 n=int(terrain.get('editor_grid_size',151));minimum=float(terrain['editor_min']);step=float(terrain['editor_step'])
 minimum_y=float(terrain.get('editor_min_y',minimum))
 u=max(0,min(n-1.00001,(x-minimum)/step));v=max(0,min(n-1.00001,(y-minimum_y)/step));i=int(u);j=int(v);a=u-i;b=v-j
 attr=terrain.data.attributes.get('BaselineHeight') if baseline else None
 def h(k):return attr.data[k].value if attr else terrain.data.vertices[k].co.z
 return h(j*n+i)*(1-a)*(1-b)+h(j*n+i+1)*a*(1-b)+h((j+1)*n+i)*(1-a)*b+h((j+1)*n+i+1)*a*b
def make_proxy(name,bounds,color):
 lo,hi=bounds;x0,y0,z0=lo;x1,y1,z1=hi
 # An eight-vertex box is deliberately independent of detailed asset geometry.
 verts=[(x,y,z) for z in [z0,z1] for y in [y0,y1] for x in [x0,x1]]
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],[(0,2,3,1),(0,1,5,4),(1,3,7,5),(3,2,6,7),(2,0,4,6),(4,5,7,6)])
 material=bpy.data.materials.new(name+' | marker colour');material.diffuse_color=(*color,1);mesh.materials.append(material);return mesh
def cloud_nodes(mesh,dimensions,color):
 group=bpy.data.node_groups.new(mesh.name+' | low detail markers','GeometryNodeTree')
 group.interface.new_socket(name='Geometry',in_out='INPUT',socket_type='NodeSocketGeometry');group.interface.new_socket(name='Geometry',in_out='OUTPUT',socket_type='NodeSocketGeometry')
 n=group.nodes;l=group.links;input=n.new('NodeGroupInput');output=n.new('NodeGroupOutput')
 ico=n.new('GeometryNodeMeshIcoSphere');ico.inputs['Radius'].default_value=1;ico.inputs['Subdivisions'].default_value=1
 transform=n.new('GeometryNodeTransform');transform.inputs['Scale'].default_value=tuple(max(.12,d*.5) for d in dimensions);transform.inputs['Translation'].default_value=(0,0,max(.12,dimensions[2]*.5));l.new(ico.outputs['Mesh'],transform.inputs['Geometry'])
 material=bpy.data.materials.new(mesh.name+' | point marker colour');material.diffuse_color=(*color,1)
 material_node=n.new('GeometryNodeSetMaterial');material_node.inputs['Material'].default_value=material;l.new(transform.outputs['Geometry'],material_node.inputs['Geometry'])
 instances=n.new('GeometryNodeInstanceOnPoints');l.new(input.outputs['Geometry'],instances.inputs['Points']);l.new(material_node.outputs['Geometry'],instances.inputs['Instance'])
 for attr,socket in [('PlacementScale','Scale'),('PlacementRotation','Rotation')]:
  attribute=n.new('GeometryNodeInputNamedAttribute');attribute.data_type='FLOAT_VECTOR';attribute.inputs['Name'].default_value=attr;l.new(attribute.outputs['Attribute'],instances.inputs[socket])
 l.new(instances.outputs['Instances'],output.inputs['Geometry']);return group
def annotate_doors(scene,owners,terrain_height_function):
 """Rebuild hinge groups and approaches from current asset geometry/placement."""
 scene.frame_set(1);bpy.context.view_layer.update();groups={}
 for door,owner_name,owner_center,asset in owners:
  part=door.get('source_part',door.get('editor_part_id','Door'));token=identity(owner_name+'|'+part);group=identity(owner_name+'|'+re.sub(r'_[LR](?:\.\d+)?$','',part))
  door.name='Door | '+token+' | '+part;door['interactive_door']=True;door['door_id']=token;door['door_group']=group;door['entry_owner']=owner_name;door['entry_asset']=asset;door['door_label']=part.replace('_',' ').split('.')[0];door['collision_kind']='none';door['asset_role']='door'
  points=[door.matrix_world@Vector(p) for p in door.bound_box];center=sum(points,Vector())/8;hinge=door.matrix_world.translation
  box=[Vector(p) for p in door.bound_box];dx=max(p.x for p in box)-min(p.x for p in box);dy=max(p.y for p in box)-min(p.y for p in box)
  normal=Vector((1,0,0)) if dx<dy else Vector((0,1,0));out=door.matrix_world.to_3x3().inverted().transposed()@normal;out.z=0;out.normalize()
  radial=center-owner_center;radial.z=0
  if out.dot(radial)<0:out=-out
  tangent=center-hinge;tangent.z=0;sign=1 if Vector((0,0,1)).cross(tangent).dot(-out)>0 else -1
  door['door_outward']=list(out);door['door_open_angle']=sign*math.pi/2;groups.setdefault(group,[]).append((door,center,min(p.z for p in points),points,out))
 vertices=[];faces=[]
 for group,items in groups.items():
  center=sum((item[1] for item in items),Vector())/len(items);out=items[0][4];right=Vector((0,0,1)).cross(out);floor=min(item[2] for item in items)-.025
  width=max(p.dot(right) for item in items for p in item[3])-min(p.dot(right) for item in items for p in item[3]);width=max(.85,min(4.5,width+.18));length=2.5
  for _ in range(3):
   start=center+out*length;ground=terrain_height_function(start.x,start.y);length=max(2.5,min(14.,abs(floor-ground)/.38+1.4))
  start=center+out*length;landing=center+out*1.4;end=center-out*.4;startz=max(terrain_height_function(*(start+right*width*.5)[:2]),terrain_height_function(*(start-right*width*.5)[:2]))+.02
  k=len(vertices);vertices.extend([(*(start-right*width/2)[:2],startz),(*(start+right*width/2)[:2],startz),(*(landing+right*width/2)[:2],floor),(*(landing-right*width/2)[:2],floor),(*(end+right*width/2)[:2],floor),(*(end-right*width/2)[:2],floor)]);faces.extend([(k,k+1,k+2,k+3),(k+3,k+2,k+4,k+5)])
  outside=center+out*.85;outside.z=floor+.045;inside=center-out*1.1;inside.z=floor+.045
  for item in items:item[0]['entry_outside']=list(outside);item[0]['entry_inside']=list(inside);item[0]['entry_floor']=floor
 return vertices,faces,len(groups)

"""One hinge/collider contract shared by full exports and metadata-only refreshes."""
from mathutils import Vector,Matrix
C=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
def bounds(points):
 return Vector(tuple(min(p[i] for p in points) for i in range(3))),Vector(tuple(max(p[i] for p in points) for i in range(3)))
def build_door_manifest(objects):
 records=[];cache={}
 for o in objects:
  if not o.get('interactive_door'):continue
  if o.data.name not in cache:
   # A handle protrudes from only a small part of a door; including it in an
   # entire leaf-sized box artificially narrows an otherwise open doorway.
   slots=[i for i,m in enumerate(o.data.materials) if m and 'boards' in m.name.lower()]
   if not slots:slots=[i for i,m in enumerate(o.data.materials) if m and any(s in m.name.lower() for s in ['oak','wood','timber'])]
   indices={i for p in o.data.polygons if p.material_index in slots for i in p.vertices}
   points=[C@o.data.vertices[i].co for i in indices] if indices else [C@Vector(p) for p in o.bound_box]
   cache[o.data.name]=(*bounds(points),bool(indices))
  lo,hi,wood=cache[o.data.name]
  m=C@o.matrix_world@C.inverted();position,rotation,scale=m.decompose();center=m@((lo+hi)/2)
  full_lo,full_hi=bounds([C@Vector(p) for p in o.bound_box]);render_center=m@((full_lo+full_hi)/2)
  half=(hi-lo)/2;half=Vector((max(.015,half.x*abs(scale.x)-.012),max(.025,half.y*abs(scale.y)-.008),max(.015,half.z*abs(scale.z)-.012)))
  records.append({'id':o['door_id'],'node':o.name,'group':o['door_group'],'label':o['door_label'],'asset':o['entry_asset'],'owner':o['entry_owner'],'hinge':list(position),'center':list(center),'render_center':list(render_center),'half':list(half),'rotation':[rotation.x,rotation.y,rotation.z,rotation.w],'open_angle':float(o['door_open_angle']),'outward':list(C.to_3x3()@Vector(o['door_outward'])),'outside':list(C@Vector(o['entry_outside'])),'inside':list(C@Vector(o['entry_inside'])),'floor':float(o['entry_floor']),'collider_bounds_source':'wooden_panel' if wood else 'full_mesh'})
 return records

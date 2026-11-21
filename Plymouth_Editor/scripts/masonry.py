"""Build-time stone dressing: existing plinth materials, coursed bases and entry steps.
Kept out of the lightweight layout. Visual joints never become collision cracks.
"""
import bpy,math,random,collections
from mathutils import Vector
class Surface:
 def __init__(self):self.vertices=[];self.faces=[];self.slots=[];self.indices=[];self.uv=[]
 def quad(self,points,mat,uv=None):
  if mat not in self.slots:self.slots.append(mat)
  k=len(self.vertices);self.vertices.extend(points);self.faces.append(tuple(range(k,k+len(points))));self.indices.append(self.slots.index(mat));self.uv.extend(uv or [(0,0),(1,0),(1,1),(0,1)])
 def box(self,points,mat):
  for face in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:self.quad([points[i] for i in face],mat)
 def object(self,scene,name,collision='none',bake=True,hidden=False):
  if not self.faces:return None
  me=bpy.data.meshes.new(name);me.from_pydata(self.vertices,[],self.faces)
  for mat in self.slots:me.materials.append(mat)
  for face,index in zip(me.polygons,self.indices):face.material_index=index
  uv=me.uv_layers.new(name='StoneUV')
  for i,value in enumerate(self.uv):uv.data[i].uv=value
  ob=bpy.data.objects.new(name,me);scene.collection.objects.link(ob);ob['collision_kind']=collision
  if bake:ob['replacement_source']='project-generated stone dressing';ob['asset_role']='masonry'
  ob.hide_render=hidden;return ob
def flat(name,color):
 m=bpy.data.materials.get(name)
 if not m:m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(*color,1);m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=.95
 return m
def palette(template):
 parts=template['parts'];plinth=[p for p in parts if p.get('part','').startswith('02 |')];materials=[m for p in (plinth or parts) for m in p['mesh'].materials if m and any(word in m.name.lower() for word in ['stone','rubble','granite']) and not any(word in m.name.lower() for word in ['soot','mortar'])]
 return list(dict.fromkeys(materials)) or [flat('Masonry | local grey rubble',(.32,.34,.30))]
def clad_foundations(scene,palettes,height):
 mortar=flat('Masonry | recessed lime mortar',(.22,.215,.19));tiles={};count=blocks=0
 for ob in list(scene.objects):
  if not ob.name.startswith(('Fitted foundation |','Trade foundation |')) or ob.type!='MESH':continue
  count+=1;points=[ob.matrix_world@v.co for v in ob.data.vertices];assert len(points)==8,ob.name;top=points[4:8];bottom=min(p.z for p in points);ceiling=sum(p.z for p in top)/4;center=sum(top,Vector())/4;key=(math.floor(center.x/120),math.floor(center.y/120));surface=tiles.setdefault(key,Surface());stones=palettes.get(ob.get('editor_support_owner'),palettes[None]);rng=random.Random(ob.name)
  ob.data=ob.data.copy();ob.data.materials.clear();ob.data.materials.append(mortar)
  for polygon in ob.data.polygons:polygon.material_index=0
  # Retain exact, rotated collision; the shallow stone faces are visual dressing.
  ob['collision_kind']='triangles';row=0;z=bottom
  while z<ceiling-.01:
   z1=min(ceiling,z+.21+rng.uniform(-.018,.018))
   for i in range(4):
    a=top[i];b=top[(i+1)%4];direction=b-a;direction.z=0;length=direction.length;direction.normalize();normal=Vector((direction.y,-direction.x,0));s=-.26 if row%2 else 0
    while s<length-.01:
     end=min(length,s+rng.uniform(.42,.66));start=max(0,s);s=end
     if end-start<.04:continue
     middle=a+direction*((start+end)/2)
     if z1<height(middle.x,middle.y)-.06:continue
     p=a+direction*(start+.007)+normal*.019;q=a+direction*(end-.007)+normal*.019;mat=rng.choice(stones)
     face=[(p.x,p.y,z+.006),(q.x,q.y,z+.006),(q.x,q.y,z1-.006),(p.x,p.y,z1-.006)];surface.quad(face,mat);blocks+=1
     # Short returns give each block a visible top arris instead of a flat decal.
     surface.quad([face[3],face[2],(q.x-normal.x*.018,q.y-normal.y*.018,z1-.006),(p.x-normal.x*.018,p.y-normal.y*.018,z1-.006)],mat)
   row+=1;z=z1
 for key,surface in tiles.items():surface.object(scene,'Foundation masonry | '+str(key))
 return {'foundations':count,'visible_stones':blocks,'render_chunks':len(tiles),'collision':'exact original rotated foundation meshes'}
def entry_steps(scene,owners,palettes,height):
 groups=collections.defaultdict(list)
 for door,name,center,asset in owners:groups[door['door_group']].append((door,name))
 visuals={};collision=Surface();mortar=flat('Masonry | recessed lime mortar',(.22,.215,.19));flights=steps=pavers=0
 for group,items in groups.items():
  doors=[item[0] for item in items];points=[door.matrix_world@Vector(p) for door in doors for p in door.bound_box];center=sum(points,Vector())/len(points);out=Vector(doors[0]['door_outward']);right=Vector((0,0,1)).cross(out);floor=float(doors[0]['entry_floor']);width=max(.85,min(4.5,max(p.dot(right) for p in points)-min(p.dot(right) for p in points)+.18));length=2.5
  for _ in range(3):
   start=center+out*length;length=max(2.5,min(14.,abs(floor-height(start.x,start.y))/.38+1.4))
  start=center+out*length;start_z=max(height(*(start+right*width/2)[:2]),height(*(start-right*width/2)[:2]))+.02;difference=floor-start_z;count=max(1,math.ceil(abs(difference)/.16));going=(length-1.4)/count
  key=(math.floor(center.x/120),math.floor(center.y/120));visual=visuals.setdefault(key,Surface());stones=palettes.get(items[0][1],palettes[None]);rng=random.Random(group)
  def point(u,v,z):p=center+out*u+right*v;return (p.x,p.y,z)
  def slab(near,far,z,landing=False):
   nonlocal pavers
   corners=[point(u,v,z) for u,v in [(near,-width/2),(far,-width/2),(far,width/2),(near,width/2)]];low=min(height(p[0],p[1]) for p in corners)-.12;low=min(low,z-.09)
   # out cross right points up: retain this winding for walkable top faces.
   ring=corners;collision.box([(p[0],p[1],low) for p in ring]+ring,mortar)
   rows=max(1,math.ceil((far-near)/.48)) if landing else 1
   for r in range(rows):
    u0=near+(far-near)*r/rows;u1=near+(far-near)*(r+1)/rows;columns=max(1,math.ceil(width/.55))
    for c in range(columns):
     v0=-width/2+width*c/columns+.006;v1=-width/2+width*(c+1)/columns-.006;mat=rng.choice(stones);a=point(u0+.004,v0,z+.004);b=point(u1-.004,v0,z+.004);c0=point(u1-.004,v1,z+.004);d=point(u0+.004,v1,z+.004);visual.quad([a,b,c0,d],mat);pavers+=1
   # Front/side stonework hides the supporting fill down to the terrain.
   for a,b in zip(ring,ring[1:]+ring[:1]):
    va,vb=Vector(a),Vector(b);edge=vb-va;edge.z=0;run=edge.length
    if run<.01:continue
    direction=edge/run;normal=Vector((direction.y,-direction.x,0));z0=low;row=0
    while z0<z-.012:
     z1=min(z-.006,z0+.20);parts=max(1,math.ceil(run/.55))
     for j in range(parts):
      a0=va+direction*(run*j/parts+.006)+normal*.009;b0=va+direction*(run*(j+1)/parts-.006)+normal*.009;mid=(a0+b0)/2
      if z1<height(mid.x,mid.y)-.045:continue
      visual.quad([(a0.x,a0.y,z0+.006),(b0.x,b0.y,z0+.006),(b0.x,b0.y,z1-.006),(a0.x,a0.y,z1-.006)],rng.choice(stones))
     z0=z1;row+=1
  for i in range(count):slab(length-(i+1)*going,length-i*going,start_z+difference*(i+1)/count);steps+=1
  slab(-.4,1.4,floor,True);flights+=1
 collision.object(scene,'Entry approaches | stone stair collision',collision='triangles',bake=False,hidden=True)
 # Collision-only objects are normally excluded; the exporter explicitly gathers this mesh.
 collision_ob=bpy.data.objects.get('Entry approaches | stone stair collision');collision_ob['collision_only']=True;collision_ob['entry_step_collision']=True
 core=collision_ob.copy();scene.collection.objects.link(core);core.name='Entry masonry | bedding and filled stair cores';core.hide_render=False;core['collision_kind']='none';del core['entry_step_collision'];del core['collision_only']
 for key,surface in visuals.items():surface.object(scene,'Entry masonry | stone steps and flagstones '+str(key))
 return {'entrances':flights,'steps':steps,'flagstones':pavers,'max_riser':.16,'level_threshold_landing':1.4,'collision':'continuous closed step meshes; cosmetic joints excluded'}

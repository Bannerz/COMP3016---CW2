"""Session-only Blender sidebar. Launch through Open_Map_Editor.cmd."""
import bpy,os,sys,json,subprocess,uuid
from mathutils import Matrix,Quaternion
HERE=os.path.dirname(os.path.abspath(__file__));ROOT=os.path.dirname(HERE);sys.path.insert(0,HERE)
from editor_common import terrain_height,relative_matrix
CATALOG=json.load(open(os.path.join(ROOT,'catalog.json')))
def selected(context):return context.active_object
def clear_previews():
 libraries=set()
 for ob in list(bpy.context.scene.objects):
  if ob.get('editor_preview'):
   if ob.instance_collection and ob.instance_collection.library:libraries.add(ob.instance_collection.library)
   source=bpy.data.objects.get(ob.get('editor_preview_source',''))
   if source:source.hide_set(False)
   bpy.data.objects.remove(ob,do_unlink=True)
 for library in libraries:bpy.data.libraries.remove(library)
class PLY_OT_open_template(bpy.types.Operator):
 bl_idname='plymouth.open_template';bl_label='Open selected template';bl_description='Open only this detailed asset in another Blender window'
 def execute(self,context):
  ob=selected(context);key=ob.get('template_id') if ob else None
  if key not in CATALOG['assets']:self.report({'ERROR'},'Select a building marker or vegetation point object');return {'CANCELLED'}
  subprocess.Popen([bpy.app.binary_path,os.path.join(ROOT,CATALOG['assets'][key]['file']),'--python',__file__]);return {'FINISHED'}
class PLY_OT_save(bpy.types.Operator):
 bl_idname='plymouth.save';bl_label='Save this file'
 def execute(self,context):
  if context.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
  if not context.scene.get('editor_schema'):bpy.ops.file.pack_all()
  bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath,compress=True);self.report({'INFO'},'Saved. Export Map uses the latest saved templates.');return {'FINISHED'}
class PLY_OT_preview(bpy.types.Operator):
 bl_idname='plymouth.preview';bl_label='Preview selected building';bl_description='Load one detailed asset at its map position; use Clear Preview to release the instance'
 def execute(self,context):
  ob=selected(context);key=ob.get('template_id') if ob else None
  if key not in CATALOG['assets'] or ob.get('placement_cloud'):self.report({'ERROR'},'Select one building/prop marker');return {'CANCELLED'}
  clear_previews();asset=CATALOG['assets'][key];path=os.path.join(ROOT,asset['file'])
  # Reload linked data so a saved template change is visible on the next preview.
  lib=next((l for l in bpy.data.libraries if os.path.normcase(bpy.path.abspath(l.filepath))==os.path.normcase(path)),None)
  if lib:
   with context.temp_override(id=lib):bpy.ops.wm.lib_reload()
  with bpy.data.libraries.load(path,link=True) as (src,dst):dst.collections=[asset['collection']]
  preview=bpy.data.objects.new('PREVIEW | '+key,None);context.scene.collection.objects.link(preview);preview.instance_type='COLLECTION';preview.instance_collection=dst.collections[0];preview.matrix_world=ob.matrix_world;preview['editor_preview']=True;preview['editor_preview_source']=ob.name;ob.hide_set(True);return {'FINISHED'}
class PLY_OT_clear(bpy.types.Operator):
 bl_idname='plymouth.clear_preview';bl_label='Clear Preview'
 def execute(self,context):clear_previews();return {'FINISHED'}
class PLY_OT_snap(bpy.types.Operator):
 bl_idname='plymouth.snap';bl_label='Place selected markers on terrain';bl_options={'REGISTER','UNDO'}
 def execute(self,context):
  terrain=next((o for o in context.scene.objects if o.get('editor_terrain')),None)
  if not terrain:return {'CANCELLED'}
  for ob in context.selected_objects:
   key=ob.get('template_id')
   if not key or ob.get('placement_cloud'):continue
   m=ob.matrix_world.copy();p=m.translation;bottom=CATALOG['assets'][key]['bounds'][0][2]*m.to_scale().z;m.translation.z=terrain_height(terrain,p.x,p.y)+.03-bottom;ob.matrix_world=m
  return {'FINISHED'}
class PLY_OT_points(bpy.types.Operator):
 bl_idname='plymouth.adjust_points';bl_label='Adjust selected vegetation';bl_options={'REGISTER','UNDO'}
 scale_factor:bpy.props.FloatProperty(name='Size multiplier',default=1.,min=.1,max=4.)
 yaw:bpy.props.FloatProperty(name='Turn (degrees)',default=0.)
 def execute(self,context):
  ob=selected(context)
  if not ob or not ob.get('placement_cloud'):return {'CANCELLED'}
  was_edit=context.mode=='EDIT_MESH'
  if was_edit:bpy.ops.object.mode_set(mode='OBJECT')
  import math
  for i,v in enumerate(ob.data.vertices):
   if v.select:
    a=ob.data.attributes['PlacementScale'].data[i];a.vector*=self.scale_factor
    r=ob.data.attributes['PlacementRotation'].data[i];r.vector.z+=math.radians(self.yaw)
  ob.data.update()
  if was_edit:bpy.ops.object.mode_set(mode='EDIT')
  return {'FINISHED'}
 def invoke(self,context,event):return context.window_manager.invoke_props_dialog(self)
# This state belongs to the Blender session and is never saved into the map.
_export = {"process": None, "percent": 0, "message": ""}


def redraw_sidebar():
 for window in bpy.context.window_manager.windows:
  for area in window.screen.areas:
   if area.type == 'VIEW_3D':
    area.tag_redraw()


def poll_export():
 process = _export["process"]
 try:
  with open(_export["progress_path"], encoding="utf-8") as progress_file:
   status = json.load(progress_file)
  _export["percent"] = max(_export["percent"], min(99, status["percent"]))
  _export["message"] = status["message"]
 except (OSError, ValueError, KeyError):
  # The background Blender process may not have written its first update yet.
  pass

 result = process.poll()
 if result is None:
  redraw_sidebar()
  return 0.4

 try:
  os.remove(_export["progress_path"])
 except OSError:
  pass
 _export["process"] = None
 if result == 0:
  _export["percent"] = 100
  _export["message"] = "Export complete. See the export folder."
 else:
  _export["message"] = "Export failed. See export/export.log."
 redraw_sidebar()
 return None


class PLY_OT_build(bpy.types.Operator):
 bl_idname = 'plymouth.build'
 bl_label = 'Save layout and Export Map'
 bl_description = 'Export the map, doors and collision from saved templates'

 @classmethod
 def poll(cls, context):
  return bool(context.scene and context.scene.get('editor_schema')) and _export["process"] is None

 def execute(self, context):
  if context.mode != 'OBJECT':
   bpy.ops.object.mode_set(mode='OBJECT')
  clear_previews()
  bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath, compress=True)
  output = os.path.join(ROOT, 'export')
  os.makedirs(output, exist_ok=True)
  progress_path = os.path.join(output, f'.progress-{uuid.uuid4().hex}.json')
  environment = os.environ.copy()
  environment['PLYMOUTH_EXPORT_PROGRESS'] = progress_path
  command = [
      bpy.app.binary_path, '--background', bpy.data.filepath, '--python-exit-code', '1',
      '--python', os.path.join(HERE, 'assemble_map.py'), '--', '--output', output,
  ]
  try:
   with open(os.path.join(output, 'export.log'), 'w') as log:
    process = subprocess.Popen(command, env=environment, stdout=log,
        stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
  except OSError as error:
   self.report({'ERROR'}, f'Could not start export: {error}')
   return {'CANCELLED'}

  _export.update(process=process, progress_path=progress_path, percent=1,
                 message="Starting background Blender")
  bpy.app.timers.register(poll_export, first_interval=0.4, persistent=True)
  redraw_sidebar()
  return {'FINISHED'}


class PLY_PT_editor(bpy.types.Panel):
 bl_label='Plymouth Map and Templates';bl_idname='PLY_PT_editor';bl_space_type='VIEW_3D';bl_region_type='UI';bl_category='Plymouth'
 def draw(self,context):
  ui=self.layout;ob=selected(context);is_map=bool(context.scene.get('editor_schema'))
  if is_map:
   ui.label(text='Move markers; edit one template at a time.')
   if ob and ob.get('template_id'):ui.label(text=ob['template_id'])
   ui.operator('plymouth.open_template');ui.operator('plymouth.preview');ui.operator('plymouth.clear_preview');ui.operator('plymouth.snap');ui.separator()
   ui.label(text='Trees: Tab, select points, G / Shift D / X')
   ui.operator('plymouth.adjust_points');ui.separator();ui.operator('plymouth.build')
   if _export["process"] is not None:
    percent = _export["percent"]
    ui.progress(factor=percent / 100.0, type='BAR', text=f'About {percent}%')
    ui.label(text=_export["message"])
   else:
    ui.label(text=_export["message"] or 'Export after saving all template windows.')
  else:
   ui.label(text='Edit the ASSET collection; keep ASSET_ROOT.')
   ui.label(text='Door origin is its hinge. Keep door objects.')
   ui.label(text='Save here, then Export Map in the layout.')
  ui.operator('plymouth.save')
for cls in [PLY_OT_open_template,PLY_OT_save,PLY_OT_preview,PLY_OT_clear,PLY_OT_snap,PLY_OT_points,PLY_OT_build,PLY_PT_editor]:
 try:bpy.utils.register_class(cls)
 except ValueError:pass
# Library-written authoring files contain scenes but no active screen setup.
scene=next((s for s in bpy.data.scenes if s.get('editor_schema')),None)
if not scene:scene=next((s for s in bpy.data.scenes if any(o.get('editor_asset_key') for o in s.objects)),bpy.context.scene)
bpy.context.window.scene=scene
for screen in bpy.data.screens:
 for area in screen.areas:
  if area.type=='VIEW_3D':
   space=area.spaces.active;space.shading.type='SOLID';space.shading.color_type='MATERIAL';space.clip_end=12000;space.show_region_ui=True
   if scene.get('editor_schema'):
    space.region_3d.view_distance=650;space.region_3d.view_location=(-80,-100,0);space.region_3d.view_rotation=Quaternion((1,0,0,0));space.region_3d.view_perspective='ORTHO'
   else:
    space.region_3d.view_distance=25;space.region_3d.view_location=(0,0,4)
print('PLYMOUTH_EDITOR_READY',scene.name,flush=True)

# Plymouth map source

Open **Open_Map_Editor.cmd** with Blender 5.1 installed. The **Plymouth** tab in the 3D View sidebar lets you open a selected template, preview it and edit the lightweight layout. Save individual templates before using **Save layout and Export Map**.

The export writes the textured world GLB, terrain, collision and placement metadata to `Plymouth_Editor/export`. The exporter also generates the original ambience clips. Your own game can use the exported files as needed.

`Map_Layout.blend` is the editable map. `Templates` contains the models and their materials. `World_Environment.blend` supplies the detailed terrain and static scenery during export. Keep these files and `catalog.json` together.

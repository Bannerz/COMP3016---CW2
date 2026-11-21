@echo off
start "Plymouth Map Editor" "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" "%~dp0Plymouth_Editor\Map_Layout.blend" --python "%~dp0Plymouth_Editor\scripts\editor_ui.py"

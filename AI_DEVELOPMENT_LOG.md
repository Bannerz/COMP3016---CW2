# AI development log

**Project** — Battle for Plymouth, COMP3016 Coursework 2  
**Log created** — 21 November 2026  
**Tool** — ChatGPT/Codex, including Blender Python for scene and mesh editing  
**Approach** — Iterative map development, with AI assistance for implementation, asset integration and verification

## Development approach

I set the direction for an explorable map inspired by late medieval Plymouth and asked for a workflow that would let me edit the layout and building designs on my PC. I chose the assets to integrate, identified problems with the houses and approaches, and directed revisions. AI wrote the Blender tools, generated and repaired geometry, placed assets, and ran the checks described below.

The prompt summaries are reconstructed examples of the requests that guided the work. They are representative paraphrases. The starting point for this repository is the editable map and its offline tools. I wrote the initial C++ renderer and movement code with AI guidance. Later entries record where I asked AI to implement or fix features directly and how the results were checked.

## Development record

### AI-01 — Terrain and settlement meshes

**Objective** — Turn a modern elevation reference into an explorable landscape inspired by Plymouth around the mid to late 1400s.

**Representative prompt** — Use the terrain and research notes to suggest a medieval shoreline and settlement. Add routes, farmland and village features that make the map interesting to explore.

**AI contribution** — Used Blender scripts to adapt the modern terrain, shape hills and channels, and generate roads, paths, fields, boundaries and early building forms. Added farmyards, trade locations, bridges, streams and small scenery such as wells and vegetation. Later passes replaced selected placeholders with more detailed models.

**Refinement and verification** — My feedback expanded the initial town to include smaller settlements and more varied terrain. The landscape and fictional destinations are artistic interpretations based on the project references.

**References** — [Map layout](Plymouth_Editor/Map_Layout.blend), [detailed environment](Plymouth_Editor/World_Environment.blend).

### AI-02 — Supplied meshes and reusable templates

**Objective** — Replace simple placeholders with detailed models and keep each design editable in one place.

**Representative prompt** — Integrate the supplied houses, church, trees and trade buildings and distribute the taverns through the town. Keep a reusable source for each design.

**AI contribution** — Imported and organised the supplied house, church, tree, tavern and civic/trade packs. Reused their meshes and materials, fitted models to existing plots, preserved separate door leaves and hinge origins, and distributed variations across the map. AI generated the integration and placement work. The supplied model geometry remains attributable to the source assets, with subsequent geometry changes described below.

**Refinement and verification** — Earlier exports checked asset counts and terrain preservation. Visual review identified leftover placeholders and detached panels, which were removed from the export. The editor migration ultimately packaged 63 shared templates.

**References** — [Template catalogue](Plymouth_Editor/catalog.json), [template files](Plymouth_Editor/Templates/).

### AI-03 — Lightweight Blender map editor

**Objective** — Make the map responsive to edit and let one saved template change update every placement of that design.

**Representative prompt** — Split the crowded Blender scene into a light layout and individual design files. Let me move placements, open and preview one design, and export the full scene afterwards.

**AI contribution** — Split the world into a layout, a detailed environment and template files indexed by a catalogue. Building markers store the template identifier and placement transform. Vegetation uses point clouds with per-point rotation, scale and identity. The sidebar opens or previews a selected template, adjusts vegetation and helps place building markers on the ground. The coarse terrain grid stores its original elevations, allowing the exporter to transfer height edits onto the detailed mesh.

**Refinement and verification** — The migration recorded 16,612 placements. The layout stored about 455,000 vertices, compared with about 793 million visible vertices in the old scene when repeated instances were counted. An isolated test edit reached every instance of the chosen house template. Tests also checked duplicate identities, moving doors and foundations with their building, and a quest marker following its mill. Initial tests caught a stale parent transform and a duplicate that could claim an original door identity. AI corrected both. Opening and clearing a linked template preview was verified on the copied map kit.

**References** — [Editor sidebar](Plymouth_Editor/scripts/editor_ui.py), [shared editor helpers](Plymouth_Editor/scripts/editor_common.py), [map assembly](Plymouth_Editor/scripts/assemble_map.py).

### AI-04 — House mesh repairs

**Objective** — Repair awkward stairs and exterior framing, and connect each chimney to a usable-looking hearth.

**Representative prompt** — Inspect the townhouse, merchant house, narrow row house and weaver's cottage. Rebuild the inaccessible stair layouts and give each house a coherent fireplace and chimney.

**AI contribution** — Restored separate named meshes from the supplied geometry and preserved the existing door pivots. Used Blender Python to construct full-depth treads, risers, stringers and rails. Recut upper floorboards and joists around the stair openings in the three two-storey designs. Refitted diagonal braces and added masonry hearths, smoke hoods, hollow flues, roof openings and flashing across the four designs. The weaver's cottage remained single-storey.

**Refinement and verification** — I questioned whether the first stair layouts were accessible. AI moved the flights inward using the ground-floor footprint and checked headroom above 46 treads and a clear path through each flue. In the earlier prototype, two full-map stair failures came from neighbouring foundations whose collision boxes extended past the visible stonework. Using the actual foundation mesh for collision resolved both. The automated capsule passed standing ascent and descent on 399 placed staircases. The geometry is AI-scripted and uses the supplied materials.

**References** — [Medieval Townhouse](Plymouth_Editor/Templates/House_Medieval_Townhouse.blend), [Merchant House](Plymouth_Editor/Templates/House_Merchant_House.blend), [Narrow Row House](Plymouth_Editor/Templates/House_Narrow_Row_House.blend), [Weaver's Cottage](Plymouth_Editor/Templates/House_Weavers_Cottage.blend).

### AI-05 — Stone foundations and entrance meshes

**Objective** — Tie the foundations visually to the building bases and replace sloped entrance ramps with masonry steps suited to the setting.

**Representative prompt** — Match exposed foundation stone to the building plinth. Use shallow steps and flagstone landings at entrances, with space to open the doors.

**AI contribution** — Wrote a mesh generator for visible stone courses, recessed mortar, treads, risers and flagstones. It samples the terrain and uses the associated building's stone palette. It groups the visual geometry by map tile and exports solid step collision separately so cosmetic joints do not interrupt walking. Foundation collision follows the rotated foundation mesh.

**Refinement and verification** — AI reviewed Blender renders of a sample frontage and entrance. The earlier full map generated dressing for 934 foundations and rebuilt 949 entrance approaches. That build passed 949 automated doorway checks and 399 interior stair checks. The checks cover scripted threshold crossings and stair traversal.

**References** — [Masonry mesh generator](Plymouth_Editor/scripts/masonry.py), [door metadata](Plymouth_Editor/scripts/door_metadata.py).

### AI-06 — Standalone map export for the fresh repository

**Objective** — Carry the editable map into a clean repository so I can implement the game code myself.

**Representative prompt** — Transfer the current Blender layout, detailed environment and templates. Keep the Blender tools needed to edit and export those assets.

**AI contribution** — Adapted the sidebar to run the map assembler directly in background Blender. The assembler resolves saved template geometry and modifiers, places shared meshes, and regenerates dependent collision and masonry. The exporter writes a static GLB, terrain, collision and placement metadata. Buildings and masonry preserve compatible UV image materials. Terrain and dense vegetation use vertex colours. The transfer includes the map source files and offline tools required for this workflow.

**Refinement and verification** — The transfer was checked against file hashes and all 63 catalogue paths. Preview and clear operations were tested on the prepared copy. Textured export, door interaction and quest/render tests were exercised in the earlier project. Those results describe the prototype used to verify the assets.

**References** — [Editor launcher](Open_Map_Editor.cmd), [world exporter](Plymouth_Editor/scripts/export_world.py), [repository setup](README.md).


### AI-07 — OpenGL setup, shaders and indexed geometry (21–22 November 2026)

**Objective** — Get a small OpenGL 4.1 scene running in Visual Studio and understand the draw path before importing assets.

**Representative prompt** — Help me configure the graphics loader and work through external shaders and indexed drawing in small steps.

**AI contribution** — Inspected the project and build output, diagnosed the missing GLEW include directory and overlapping GLAD/GLEW setup, and explained repo-relative include and link paths. Supplied example code for compiling and linking the GLSL files, uploading a vertex buffer and index buffer, and enabling depth testing. I entered and integrated the examples in the new project and ran the builds.

**Refinement and verification** — I confirmed the window opened, both shaders compiled, and the orange triangle drew. The next iteration reused rectangle corners through an index buffer and used eight vertices and 36 indices to draw a box. A yaw uniform driven by SDL time rotated it. The fragment shader estimated surface normals from position derivatives and shaded the faces.

**References** — `e422c6b` (project setup) and `c8de8ca` (rotating box). The current shader stages are `basic.vert` and `basic.frag` in the Visual Studio project directory.

### AI-08 — Perspective and keyboard camera controls (22–23 November 2026)

**Objective** — Replace the flat preview with perspective and move the camera along its viewing axis.

**Representative prompt** — Help me add a perspective camera and make W and S move it at a consistent speed.

**AI contribution** — Provided a GLSL perspective calculation using the viewport aspect ratio and explained the perspective divide. Helped connect the camera distance as a uniform and use elapsed frame time for keyboard movement. When the box appeared to squash, AI inspected my shader and identified a multiplication where the two terms of the Y-rotation calculation should have been added. When the keyboard state call failed to compile, AI checked the local SDL3 header and corrected the example to pass `nullptr` for the unused key count.

**Refinement and verification** — I corrected the rotation and confirmed the spinning shape looked right. After fixing the keyboard call, I reported that the camera controls worked. These were developer run checks on the new renderer.

**References** — `8294e51` (rotation and perspective) and `79841cd` (keyboard camera distance).

### AI-09 — Assimp model loading and the sword texture (23 November 2026)

**Objective** — Replace hard-coded box geometry with a Blender export and display its base colour in the shader.

**Representative prompt** — Help me build Assimp from the supplied source, draw my sword GLB and use the model's UVs to texture it.

**AI contribution** — Inspected the GLB and identified the default sword scene, geometry counts, UVs and embedded PNG maps. Provided build and link guidance for Assimp, then snippets for putting its positions and triangle indices into the existing OpenGL buffers. AI extracted the sword's base-colour PNG from the GLB and converted its RGB pixels losslessly to a BMP, allowing the first texture test to use SDL3. I integrated the vertex layout, texture upload and shader sampler. AI diagnosed the missing `swordTexture` declaration when it stopped the build.

**Refinement and verification** — My console screenshot showed one imported mesh with 2,430 vertices and 2,848 faces. I then confirmed the sword appeared and the texture worked. The new game draws the sword's base colour from the BMP. Normal and metallic/roughness maps are still embedded in the GLB for future rendering work.

**References** — `675db2c` (Assimp loading), `e486d4f` (base-colour texture), [sword GLB](sword_test_model.glb) and [BMP texture](Sword_BaseColor.bmp).

### AI-10 — Full map rendering and fly camera (24–25 November 2026)

**Objective** — Render the exported town and navigate the scene using the existing shader program.

**Representative prompt** — Generalise the sword importer to draw the Plymouth GLB and let me move and look around the result.

**AI contribution** — Supplied an example map loader in `MapPreview.h`. It uploads each Assimp mesh once, walks the node hierarchy with the accumulated parent transform, and draws the mesh at each recorded placement. The preview combines vertex colours with material tint. AI guided the change from an overhead view to a perspective fly camera and provided examples for relative mouse motion, pitch limits, keyboard movement and cursor capture. I integrated the examples and tested the scene. At my request, AI later edited comments in the four renderer source files and checked that the executable statements were unchanged.

**Refinement and verification** — I recognised the town in the overhead render and confirmed that the fly camera worked. At this stage, the map preview drew individual placements using material tint and vertex colours. The sword BMP was the first image texture in the new renderer. AI-16 records the later building texture integration.

**References** — `38add94` (overhead map preview), `303e608` (interactive perspective camera) and `1c6d0a6` (rendering comments).

### AI-11 — PhysX terrain, jumping and fly toggle (25 November 2026)

**Objective** — Add a grounded player controller and keep flight available for inspecting the map.

**Representative prompt** — Walk me through building the vendored PhysX libraries and connecting a player capsule to the camera, with jumping and a fly-mode toggle.

**AI contribution** — Provided a CPU-only CMake configuration for the vendored PhysX source, help finding the Debug x64 libraries, and example code for a `PlayerPhysics` class and its integration in `main.cpp`. The class reads the exported height samples, arranges them for PhysX and builds a heightfield. A capsule controller handles movement. The code applies gravity, uses collision flags to determine ground contact, and gives the capsule an upward velocity on a new Space press while grounded. I added the class and build setup, wired the controls, and ran the game. F2 switches from the fly camera to following the capsule's feet plus eye height. Mouse look is shared by both modes.

**Refinement and verification** — I reported that the new movement worked. AI inspected the checked-in source, confirmed that it reads `terrain.bin`, and verified that the exported terrain is a 600 × 600 grid. This stage handled terrain collision. AI-13 records the later integration of `colliders.bin` and `interiors.bin`. The earlier doorway and stair audits relate to the prototype.

**References** — `c8da488` (PhysX terrain walking controls), `PlayerPhysics.h`, and `PlymBattle/Battle For Plymouth/physics/CMakeLists.txt`.

### AI-12 — Repository troubleshooting (21 November 2026)

**Objective** — Publish the initial commit with the map and renderer sources.

**Representative prompt** — Identify why GitHub Desktop cannot publish the initial commit and help remove generated files that block the push.

**AI contribution** — Explained why Visual Studio locks files under `.vs` and distinguished the Windows line-ending warnings from the staging error. Inspected the later Git LFS failure and found 32 pointers in PhysX's unused Flow build folder. AI made a local backup branch, amended the first local commit to remove the pointers from tracking, and added a targeted ignore rule. The local Flow files were retained.

**Refinement and verification** — AI checked that no LFS pointers remained on the amended main branch and that the ignore rule matched. I handled the publish step in GitHub Desktop. Later repository inspection showed `main` tracking `origin/main`.

**References** — `e422c6b` and [.gitignore](.gitignore).

### AI-13 — Static world and building collision (26 November 2026)

**Objective** — Use the exported collision geometry to walk around walls, enter buildings and use their interiors.

**Representative prompt** — Help me load the static map colliders into PhysX, diagnose why the capsule gets stuck when landing on the boundary wall by the farm gate and add building interiors.

**AI contribution** — Provided guidance and C++ examples for loading box colliders and cooking the exported triangle meshes. I integrated the collider loading into the walking controller and tried the changes in-game. After I identified the wall by the signed Brookfield gate, AI traced the remaining failure to inward-facing collision triangles. AI corrected the triangle winding for farm enclosure segments and dry-stone field walls in the exporter and regenerated the collision data. For interiors, AI provided code and guidance to cook each shared template mesh once and create static PhysX actors with the exporter’s placement, rotation and scale. The exported shared geometry leaves openings for the separate door leaves.

**Refinement and verification** — An isolated PhysX check verified that the capsule could land on and walk off both tested sections of the Brookfield enclosure. I tested the building collision in-game and reported that it worked. The current export contains 8,115 box colliders, 1,196,042 static collision triangles and 899 interior instances built from six shared meshes.

**References** — [World exporter](Plymouth_Editor/scripts/export_world.py), [map assembly](Plymouth_Editor/scripts/assemble_map.py), [PlayerPhysics.h](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/PlayerPhysics.h>).

### AI-14 — Blender export progress (26 November 2026)

**Objective** — Show that the background map export is still running and indicate its current stage.

**Representative prompt** — Add a progress bar to the Blender map editor so I can see where a long export has reached.

**AI contribution** — Added progress updates for the background Blender process and a sidebar timer that reads them. The bar shows an approximate percentage and current stage, including template loading, collision export, render mesh preparation and GLB writing. The editor displays completion when the child process exits successfully and points to the export log on failure.

**Refinement and verification** — I re-exported the map during the wall collision investigation. The export progress comes from stage updates and template counts. The GLB writer reports its stage as a single operation.

**References** — [Editor sidebar](Plymouth_Editor/scripts/editor_ui.py), [progress reporting](Plymouth_Editor/scripts/export_progress.py), [world exporter](Plymouth_Editor/scripts/export_world.py).

### AI-15 — Interactive doors (26 November 2026)

**Objective** — Let the player open and close building doors while keeping the visible leaves and their collision aligned.

**Representative prompt** — Walk me through a single working door first. Check my integration, then use the exported metadata to enable the same interaction across the map.

**AI contribution** — Provided hinge rotation and collider code for the Brookfield farmhouse door. I added the first version and asked AI to check where I had placed the code. AI moved the callable methods into the public section of `PlayerPhysics` and added collider cleanup. After I confirmed the test door worked, AI implemented the map-wide version. It reads hinge, closed pose, collider size, swing angle and group ID from `world.json`, matches each leaf to its Assimp node, and creates a kinematic box collider. In walk mode, E toggles the nearest doorway in reach. The code checks for a player overlap on every leaf before moving the colliders and applying the same hinge rotation to the render meshes.

**Refinement and verification** — I confirmed the single-door implementation in-game. The map-wide Debug build succeeded. An automated test loaded all 949 doorways and 964 leaves from the current export. It verified the Brookfield threshold, a rotated door and a two-leaf door, including both leaves staying open when one is obstructed. AI did not exercise the map-wide E key input in the game window during that test.

**References** — [MapDoors.h](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/MapDoors.h>), [MapPreview.h](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/MapPreview.h>), [PlayerPhysics.h](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/PlayerPhysics.h>), [door metadata exporter](Plymouth_Editor/scripts/door_metadata.py).

### AI-16 — Building base-colour textures (26 November 2026)

**Objective** — Show the authored building materials in the OpenGL game and add surface detail to older meshes without UVs.

**Representative prompt** — Make the buildings use their exported image textures in-game and give the remaining foundations, steps and similar surfaces a matching finish.

**AI contribution** — AI inspected the existing GLB and found that the exporter already preserved the architectural images and UVs. AI adapted the earlier prototype’s Windows Imaging Component decoder for the current renderer, wired Assimp’s base-colour references and UVs into the mesh upload, and reused the decoded OpenGL textures across placements. The fragment shader samples the authored textures in linear colour and converts the lit result for display. Selected materials without UVs use shared images projected in world space or a shader pattern for cloth and thatch. The work reused the existing GLB and its packed images. This implementation covers base colour. Normal maps and roughness are still pending.

**Refinement and verification** — The Debug build succeeded. A hidden OpenGL 4.1 test loaded 38 GPU textures, reported 3,097 UV-textured meshes and 3,345 meshes with fallback surface detail, and rendered the same Brookfield view with texturing enabled and disabled. AI inspected the comparison images and checked that OpenGL reported no errors after rendering and cleanup.

**References** — [MapTextures.h](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/MapTextures.h>), [MapPreview.h](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/MapPreview.h>), [vertex shader](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/basic.vert>), [fragment shader](<PlymBattle/Battle For Plymouth/Battle_For_Plymouth/basic.frag>).

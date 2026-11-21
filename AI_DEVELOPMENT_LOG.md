# AI development log

**Project** — Battle for Plymouth, COMP3016 Coursework 2  
**Log created** — 21 November 2026  
**Tool** — ChatGPT/Codex, including Blender Python for scene and mesh editing  
**Approach** — Iterative map development, with AI assistance for implementation, asset integration and verification

## Development approach

I set the direction for an explorable map inspired by late medieval Plymouth and asked for a workflow that would let me edit the layout and building designs on my PC. I chose the assets to integrate, identified problems with the houses and approaches, and directed revisions. AI wrote the Blender tools, generated and repaired geometry, placed assets, and ran the checks described below.

The prompt summaries are reconstructed examples of the requests that guided the work. They are representative paraphrases. The starting point for this repository is the editable map and its offline tools. The automated gameplay results below came from the earlier prototype in the original workspace. I plan to build the new game's C++ code by hand and will record that work as it happens.

## Development record

### AI-01 — Terrain and settlement meshes

**Objective** — Turn a modern elevation reference into an explorable landscape inspired by Plymouth around the mid to late 1400s.

**Representative prompt** — Use the terrain and research notes to suggest a medieval shoreline and settlement. Add routes, farmland and village features that make the map interesting to explore.

**AI contribution** — Used Blender scripts to adapt the modern terrain, shape hills and channels, and generate roads, paths, fields, boundaries and early building forms. Added farmyards, trade locations, bridges, streams and small scenery such as wells and vegetation. Later passes replaced selected placeholders with more detailed models.

**Refinement and verification** — My feedback expanded the initial town to include smaller settlements and more varied terrain. The original workspace used scene checks and rendered views to inspect the results. The landscape and fictional destinations are artistic interpretations based on the project references.

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

**Objective** — Carry the editable map into a clean repository so I can implement the new game code myself.

**Representative prompt** — Transfer the current Blender layout, detailed environment and templates. Keep the Blender tools needed to edit and export those assets.

**AI contribution** — Adapted the sidebar to run the map assembler directly in background Blender. The assembler resolves saved template geometry and modifiers, places shared meshes, and regenerates dependent collision and masonry. The exporter writes a static GLB, terrain, collision and placement metadata. Buildings and masonry preserve compatible UV image materials. Terrain and dense vegetation use vertex colours. The transfer includes the map source files and offline tools required for this workflow.

**Refinement and verification** — The transfer was checked against file hashes and all 63 catalogue paths. Preview and clear operations were tested on the prepared copy. Textured export, door interaction and quest/render tests were exercised in the earlier project. Those results describe the prototype used to verify the assets.

**References** — [Editor launcher](Open_Map_Editor.cmd), [world exporter](Plymouth_Editor/scripts/export_world.py), [repository setup](README.md).

import bpy
from pathlib import Path

OUT = Path(__file__).resolve().parent / "walkie_radio.obj"

radio = bpy.data.objects.get("walkie_radio")
if radio is None:
    raise SystemExit("walkie_radio object not found")

bpy.context.view_layer.objects.active = radio
radio.select_set(True)

# Triangulate only this in-memory copy of the scene.
mod = radio.modifiers.new("Cook_Triangulate", "TRIANGULATE")
mod.quad_method = "BEAUTY"
mod.ngon_method = "BEAUTY"

bpy.ops.object.modifier_apply(modifier=mod.name)

# Export the cooked-source OBJ.
bpy.ops.wm.obj_export(
    filepath=str(OUT),
    export_selected_objects=True,
    export_materials=False,
)

print(f"Wrote triangulated OBJ: {OUT}")
print(f"Vertices: {len(radio.data.vertices)}")
print(f"Polygons: {len(radio.data.polygons)}")

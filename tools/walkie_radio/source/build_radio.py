import bpy
import math
from pathlib import Path

OUT = Path(__file__).resolve().parent

# Start clean.
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

parts = []

def add_box(name, loc, scale, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    if bevel > 0:
        mod = o.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2

    parts.append(o)
    return o

def add_cylinder(name, loc, radius, depth, vertices=12, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=depth,
        location=loc,
        rotation=rotation,
    )
    o = bpy.context.object
    o.name = name
    parts.append(o)
    return o


# Dimensions are in Blender meters.
# Target physical size: roughly 18 cm tall body.

# Main body
add_box(
    "Radio_Body",
    (0, 0, 0.09),
    (0.032, 0.018, 0.09),
    bevel=0.004,
)

# Slightly raised front face
add_box(
    "Front_Panel",
    (0, -0.019, 0.105),
    (0.026, 0.003, 0.055),
    bevel=0.002,
)

# Antenna
add_cylinder(
    "Antenna",
    (-0.020, 0, 0.235),
    0.006,
    0.115,
    vertices=10,
)

# Antenna base
add_cylinder(
    "Antenna_Base",
    (-0.020, 0, 0.184),
    0.009,
    0.018,
    vertices=12,
)

# Top power/volume knob
add_cylinder(
    "Power_Knob",
    (0.017, 0, 0.190),
    0.010,
    0.018,
    vertices=12,
)

# Speaker grille bars
for i in range(7):
    z = 0.125 - i * 0.009
    add_box(
        f"Speaker_{i}",
        (0, -0.023, z),
        (0.018, 0.002, 0.002),
        bevel=0.0005,
    )

# Side PTT button
add_box(
    "PTT",
    (0.035, 0, 0.112),
    (0.004, 0.010, 0.021),
    bevel=0.002,
)

# Small status LED
add_box(
    "Status_LED",
    (0.018, -0.0235, 0.164),
    (0.003, 0.0015, 0.003),
    bevel=0.0005,
)

# Small lower label plate
add_box(
    "Label_Plate",
    (0, -0.023, 0.046),
    (0.018, 0.0015, 0.010),
    bevel=0.001,
)

# Join everything into one mesh.
bpy.ops.object.select_all(action="DESELECT")
for o in parts:
    o.select_set(True)

bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.convert(target="MESH")
bpy.ops.object.join()

radio = bpy.context.object
radio.name = "walkie_radio"

# Apply all remaining modifiers.
bpy.context.view_layer.objects.active = radio
for mod in list(radio.modifiers):
    bpy.ops.object.modifier_apply(modifier=mod.name)

# Smooth antenna/knobs while keeping body edges usable.
for poly in radio.data.polygons:
    poly.use_smooth = False

# Put origin near bottom center.
bpy.context.scene.cursor.location = (0, 0, 0)
bpy.ops.object.origin_set(type="ORIGIN_CURSOR")

# Save editable Blender source.
blend_path = OUT / "walkie_radio.blend"
bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

# Export OBJ for the cooker.
obj_path = OUT / "walkie_radio.obj"
bpy.ops.wm.obj_export(
    filepath=str(obj_path),
    export_materials=False,
)

print(f"Wrote: {blend_path}")
print(f"Wrote: {obj_path}")
print(f"Verts: {len(radio.data.vertices)}")
print(f"Faces: {len(radio.data.polygons)}")

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

# Snap Blender does not expose ~/.local Python packages.
sys.path.insert(0, str(ROOT / "tools" / "walkie_radio" / "pydeps"))
sys.path.insert(0, str(ROOT / "tools" / "blender"))

from votvio.ue_provider import open_game

PAKS = "/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Content/Paks"
ASSET = "VotV/Content/meshes/atvupgrades/atvRadio_radio_prop"

print("Mounting VotV pak...")
game = open_game(PAKS)

print("Loading:", ASSET)
mesh = game.find_export(ASSET, "StaticMesh")

if mesh is None:
    print("ERROR: StaticMesh export not found")
    if game.warnings:
        print("\nWarnings:")
        for w in game.warnings:
            print(" ", w)
    raise SystemExit(1)

print("\nStaticMesh loaded successfully")
print("LODs:", len(mesh.LODs))
print("Materials:", len(mesh.Materials))

for i, lod in enumerate(mesh.LODs):
    print(f"\nLOD {i}")
    print("  cooked out:", lod.is_lod_cooked_out)
    print("  inlined:", lod.inlined)
    print("  sections:", len(lod.sections))

    if lod.positionVertexBuffer:
        print(
            "  positions:",
            lod.positionVertexBuffer.NumVertices,
            "stride:",
            lod.positionVertexBuffer.Stride,
        )

    if lod.vertexBuffer:
        print(
            "  vertices:",
            lod.vertexBuffer.NumVertices,
            "uv sets:",
            lod.vertexBuffer.NumTexCoords,
            "full precision UV:",
            lod.vertexBuffer.UseFullPrecisionUVs,
        )

    if lod.indexBuffer:
        inds = (
            lod.indexBuffer.indices32
            if lod.indexBuffer.indices32
            else lod.indexBuffer.indices16
        )
        print("  indices:", len(inds))
        print("  triangles:", len(inds) // 3)
        print("  index width:", 32 if lod.indexBuffer.indices32 else 16)

if mesh.Bounds is not None:
    try:
        print("\nBounds:", mesh.Bounds.GetValue())
    except Exception:
        print("\nBounds object:", mesh.Bounds)

if game.warnings:
    print("\nParser warnings:")
    for w in game.warnings:
        print(" ", w)

print("\n--- RAW EXPORT MAP ---")

pkg = game.load_package(ASSET)

for i, exp in enumerate(pkg.ExportMap):
    try:
        name = exp.name.string
    except Exception:
        name = "<unknown>"

    try:
        typ = exp.type.string
    except Exception:
        typ = "<unknown>"

    print(
        f"Export {i}: "
        f"name={name} "
        f"type={typ} "
        f"offset={exp.SerialOffset} "
        f"size={exp.SerialSize}"
    )

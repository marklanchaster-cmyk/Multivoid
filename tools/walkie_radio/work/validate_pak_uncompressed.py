import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

sys.path.insert(0, str(ROOT / "tools/walkie_radio/pydeps"))
sys.path.insert(0, str(ROOT / "tools/blender"))

from votvio.ue_provider import open_game

PAKS = str(ROOT / "tools/walkie_radio/work/pak_validation")
ASSET = "VotV/Content/Mods/VOTVCoop/walkie/atvRadio_radio_prop"

print("Mounting generated pak...")
game = open_game(PAKS)

print("Loading:", ASSET)
mesh = game.find_export(ASSET, "StaticMesh")

if mesh is None:
    print("ERROR: StaticMesh not found")
    for w in game.warnings:
        print(" ", w)
    raise SystemExit(1)

lod = mesh.LODs[0]

indices = (
    lod.indexBuffer.indices32
    if lod.indexBuffer.indices32
    else lod.indexBuffer.indices16
)

print()
print("PAK ASSET LOAD SUCCESS")
print("LODs:", len(mesh.LODs))
print("vertices:", lod.positionVertexBuffer.NumVertices)
print("UV sets:", lod.vertexBuffer.NumTexCoords)
print("indices:", len(indices))
print("triangles:", len(indices) // 3)
print("bounds:", mesh.Bounds.GetValue())

assert lod.positionVertexBuffer.NumVertices == 3388
assert len(indices) == 4260
assert len(indices) // 3 == 1420

print()
print("FINAL PAK VALIDATION: PASS")

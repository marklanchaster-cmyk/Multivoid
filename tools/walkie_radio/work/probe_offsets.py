import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

sys.path.insert(0, str(ROOT / "tools" / "walkie_radio" / "pydeps"))
sys.path.insert(0, str(ROOT / "tools" / "blender"))

from votvio.ue_provider import open_game

from UE4Parse.Assets.Exports.StaticMesh.FStaticMeshLODResources import (
    FStaticMeshLODResources,
)
from UE4Parse.Assets.Exports.StaticMesh.FStaticMeshSection import (
    FStaticMeshSection,
)
from UE4Parse.Assets.Exports.StaticMesh.FPositionVertexBuffer import (
    FPositionVertexBuffer,
)
from UE4Parse.Assets.Exports.StaticMesh.FStaticMeshVertexBuffer import (
    FStaticMeshVertexBuffer,
)
from UE4Parse.Assets.Objects.Meshes.FColorVertexBuffer import (
    FColorVertexBuffer,
)
from UE4Parse.Assets.Exports.StaticMesh.FRawStaticIndexBuffer import (
    FRawStaticIndexBuffer,
)


PAKS = "/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Content/Paks"
ASSET = "VotV/Content/meshes/atvupgrades/atvRadio_radio_prop"

counts = {}


def positions(reader):
    local = reader.tell()

    try:
        absolute = reader.absolute_position
    except Exception:
        absolute = local

    return local, absolute


def wrap(cls, label, describe=None):
    original = cls.__init__

    def hooked(self, reader, *args, **kwargs):
        n = counts.get(label, 0)
        counts[label] = n + 1

        local0, abs0 = positions(reader)

        original(self, reader, *args, **kwargs)

        local1, abs1 = positions(reader)

        extra = ""
        if describe:
            try:
                extra = "  " + describe(self)
            except Exception as e:
                extra = f"  <describe failed: {e}>"

        print(
            f"TRACE {label}[{n}] "
            f"local={local0}->{local1} "
            f"abs={abs0}->{abs1} "
            f"bytes={local1-local0}"
            f"{extra}"
        )

    cls.__init__ = hooked


wrap(
    FStaticMeshSection,
    "Section",
    lambda x:
        f"first={x.FirstIndex} tris={x.NumTriangles} "
        f"verts={x.MinVertexIndex}..{x.MaxVertexIndex}"
)

wrap(
    FPositionVertexBuffer,
    "PositionBuffer",
    lambda x:
        f"verts={x.NumVertices} stride={x.Stride}"
)

wrap(
    FStaticMeshVertexBuffer,
    "VertexBuffer",
    lambda x:
        f"verts={x.NumVertices} uvs={x.NumTexCoords} "
        f"fullUV={x.UseFullPrecisionUVs} "
        f"hiTangent={x.UseHighPrecisionTangentBasis}"
)

wrap(
    FColorVertexBuffer,
    "ColorBuffer",
    lambda x:
        f"verts={x.numVertices} stride={x.stride}"
)

wrap(
    FRawStaticIndexBuffer,
    "IndexBuffer",
    lambda x:
        f"indices={len(x.indices32) if x.indices32 else len(x.indices16)} "
        f"width={32 if x.indices32 else 16}"
)

wrap(
    FStaticMeshLODResources,
    "LOD",
    lambda x:
        f"sections={len(x.sections)} "
        f"inlined={x.inlined} "
        f"cookedOut={x.is_lod_cooked_out}"
)


print("Mounting VotV pak...")
game = open_game(PAKS)

print("Loading:", ASSET)
mesh = game.find_export(ASSET, "StaticMesh")

if mesh is None:
    raise SystemExit("StaticMesh not found")

print("\nDONE")

#!/usr/bin/env python3

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

sys.path.insert(0, str(ROOT / "tools" / "walkie_radio" / "pydeps"))
sys.path.insert(
    0,
    str(ROOT / "tools" / "blender" / "votvio" / "vendor")
)

from UE4Parse.BinaryReader import BinaryStream
from UE4Parse.Assets.PackageReader import LegacyPackageReader
from UE4Parse.Versions import EUEVersion, VersionContainer


GEN = ROOT / "tools/walkie_radio/work/template"

UA = GEN / "atvRadio_radio_prop.uasset"
UX = GEN / "atvRadio_radio_prop.uexp"


class ProviderStub:
    def __init__(self):
        self.Versions = VersionContainer(EUEVersion.GAME_UE4_27)
        self.Triggers = {}


provider = ProviderStub()

uasset = BinaryStream(str(UA))
uexp = BinaryStream(str(UX))

# LegacyPackageReader expects this attribute to exist.
uasset.mappings = None
uexp.mappings = None

print("Opening generated package...")

pkg = LegacyPackageReader(
    uasset,
    uexp,
    None,
    provider,
)

mesh = pkg.find_export_of_type("StaticMesh")

if mesh is None:
    raise SystemExit("ERROR: StaticMesh export was not parsed")

print("StaticMesh parsed successfully")
print(f"LODs: {len(mesh.LODs)}")

if len(mesh.LODs) != 1:
    raise RuntimeError(f"Expected 1 LOD, got {len(mesh.LODs)}")

lod = mesh.LODs[0]

print()
print("LOD 0:")
print(f"  cooked out: {lod.is_lod_cooked_out}")
print(f"  inlined:    {lod.inlined}")
print(f"  sections:   {len(lod.sections)}")

if lod.positionVertexBuffer is None:
    raise RuntimeError("Position buffer missing")

if lod.vertexBuffer is None:
    raise RuntimeError("StaticMesh vertex buffer missing")

if lod.indexBuffer is None:
    raise RuntimeError("Main index buffer missing")

print(
    f"  positions:  {lod.positionVertexBuffer.NumVertices}"
)
print(
    f"  vertices:   {lod.vertexBuffer.NumVertices}"
)
print(
    f"  UV sets:    {lod.vertexBuffer.NumTexCoords}"
)

main_indices = (
    lod.indexBuffer.indices32
    if lod.indexBuffer.indices32
    else lod.indexBuffer.indices16
)

print(f"  indices:    {len(main_indices)}")
print(f"  triangles:  {len(main_indices) // 3}")

if lod.sections:
    sec = lod.sections[0]

    print()
    print("Section 0:")
    print(f"  material:     {sec.MaterialIndex}")
    print(f"  first index:  {sec.FirstIndex}")
    print(f"  triangles:    {sec.NumTriangles}")
    print(
        f"  vertex range: "
        f"{sec.MinVertexIndex}..{sec.MaxVertexIndex}"
    )

print()
print("Optional index buffers:")
print(
    "  reversed:",
    0 if lod.reversedIndexBuffer is None else
    len(
        lod.reversedIndexBuffer.indices32
        or lod.reversedIndexBuffer.indices16
    )
)
print(
    "  depth:",
    0 if lod.depthOnlyIndexBuffer is None else
    len(
        lod.depthOnlyIndexBuffer.indices32
        or lod.depthOnlyIndexBuffer.indices16
    )
)
print(
    "  reversed depth:",
    0 if lod.reversedDepthOnlyIndexBuffer is None else
    len(
        lod.reversedDepthOnlyIndexBuffer.indices32
        or lod.reversedDepthOnlyIndexBuffer.indices16
    )
)
print(
    "  adjacency:",
    0 if lod.adjacencyIndexBuffer is None else
    len(
        lod.adjacencyIndexBuffer.indices32
        or lod.adjacencyIndexBuffer.indices16
    )
)

if mesh.Bounds is not None:
    print()
    print("Bounds:")
    print(mesh.Bounds.GetValue())


# Hard validation.
assert lod.positionVertexBuffer.NumVertices == 3388
assert lod.vertexBuffer.NumVertices == 3388
assert lod.vertexBuffer.NumTexCoords == 2
assert len(main_indices) == 4260
assert len(lod.sections) == 1
assert lod.sections[0].NumTriangles == 1420
assert lod.sections[0].MaxVertexIndex == 3387

print()
print("GENERATED STATIC MESH VALIDATION: PASS")

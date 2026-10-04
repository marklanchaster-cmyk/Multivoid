#!/usr/bin/env python3

from pathlib import Path
import json
import math
import struct

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "tools/walkie_radio/work/radio_geometry.json"
OUT = ROOT / "tools/walkie_radio/work/generated"
OUT.mkdir(parents=True, exist_ok=True)

data = json.loads(SRC.read_text())
verts = data["vertices"]
indices = data["indices"]

N = len(verts)


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))


def add(a, b):
    return tuple(x+y for x, y in zip(a, b))


def mul(a, s):
    return tuple(x*s for x in a)


def cross(a, b):
    return (
        a[1]*b[2] - a[2]*b[1],
        a[2]*b[0] - a[0]*b[2],
        a[0]*b[1] - a[1]*b[0],
    )


def normalize(v):
    m = math.sqrt(dot(v, v))
    if m < 1e-12:
        return (0.0, 0.0, 0.0)
    return tuple(x/m for x in v)


# ----------------------------------------------------------------------
# Build tangent basis from geometry + UV0.
# ----------------------------------------------------------------------

tan_sum = [(0.0, 0.0, 0.0) for _ in range(N)]
bitan_sum = [(0.0, 0.0, 0.0) for _ in range(N)]

for f in range(0, len(indices), 3):
    ia, ib, ic = indices[f:f+3]

    a = verts[ia]
    b = verts[ib]
    c = verts[ic]

    p0 = tuple(a["position"])
    p1 = tuple(b["position"])
    p2 = tuple(c["position"])

    uv0 = tuple(a["uv0"])
    uv1 = tuple(b["uv0"])
    uv2 = tuple(c["uv0"])

    e1 = sub(p1, p0)
    e2 = sub(p2, p0)

    du1 = uv1[0] - uv0[0]
    dv1 = uv1[1] - uv0[1]
    du2 = uv2[0] - uv0[0]
    dv2 = uv2[1] - uv0[1]

    det = du1*dv2 - du2*dv1

    if abs(det) > 1e-12:
        r = 1.0 / det

        tangent = (
            (e1[0]*dv2 - e2[0]*dv1) * r,
            (e1[1]*dv2 - e2[1]*dv1) * r,
            (e1[2]*dv2 - e2[2]*dv1) * r,
        )

        bitangent = (
            (e2[0]*du1 - e1[0]*du2) * r,
            (e2[1]*du1 - e1[1]*du2) * r,
            (e2[2]*du1 - e1[2]*du2) * r,
        )

        for i in (ia, ib, ic):
            tan_sum[i] = add(tan_sum[i], tangent)
            bitan_sum[i] = add(bitan_sum[i], bitangent)


tangents = []

for i, v in enumerate(verts):
    n = normalize(tuple(v["normal"]))

    t = tan_sum[i]
    t = sub(t, mul(n, dot(n, t)))
    t = normalize(t)

    # Degenerate/missing UV tangent fallback.
    if dot(t, t) < 0.5:
        axis = (0.0, 0.0, 1.0)

        if abs(dot(n, axis)) > 0.9:
            axis = (0.0, 1.0, 0.0)

        t = normalize(cross(axis, n))

    b = normalize(bitan_sum[i])

    # UE stores the basis determinant sign in TangentZ.W.
    sign = -1.0 if dot(cross(n, t), b) < 0.0 else 1.0

    tangents.append((t, n, sign))


# ----------------------------------------------------------------------
# UE packed tangent helper.
#
# UE4.27 stores FPackedNormal components as signed normalized int8.
# ----------------------------------------------------------------------

def snorm8(x):
    x = max(-1.0, min(1.0, float(x)))
    q = int(round(x * 127.0))
    q = max(-127, min(127, q))
    return q


def packed_normal(v, w):
    return struct.pack(
        "<bbbb",
        snorm8(v[0]),
        snorm8(v[1]),
        snorm8(v[2]),
        snorm8(w),
    )


# ----------------------------------------------------------------------
# FPositionVertexBuffer
#
# int32 Stride
# int32 NumVertices
# BulkTArray<FVector>:
#   int32 ElementSize
#   int32 Count
#   FVector[count]
# ----------------------------------------------------------------------

position = bytearray()

position += struct.pack("<ii", 12, N)
position += struct.pack("<ii", 12, N)

for v in verts:
    position += struct.pack("<fff", *v["position"])


# ----------------------------------------------------------------------
# FStaticMeshVertexBuffer
#
# FStripDataFlags      2 bytes
# NumTexCoords         int32
# NumVertices          int32
# FullPrecisionUVs     bool/int32
# HighPrecisionTangent bool/int32
#
# Tangent bulk: item size 8, count=N
# UV bulk:      item size 4, count=N*2
# ----------------------------------------------------------------------

vertex = bytearray()

# Editor data stripped, no class-specific strip flags.
vertex += bytes((1, 0))

vertex += struct.pack(
    "<iiii",
    2,      # NumTexCoords
    N,
    0,      # UseFullPrecisionUVs
    0,      # UseHighPrecisionTangentBasis
)

# TangentX + TangentZ = 8 bytes/vertex.
vertex += struct.pack("<ii", 8, N)

for tangent, normal, sign in tangents:
    vertex += packed_normal(tangent, 0.0)
    vertex += packed_normal(normal, sign)

# Two half-float UV channels.
vertex += struct.pack("<ii", 4, N * 2)

for v in verts:
    for uv in (v["uv0"], v["uv1"]):
        vertex += struct.pack("<ee", float(uv[0]), float(uv[1]))


# ----------------------------------------------------------------------
# FRawStaticIndexBuffer
# ----------------------------------------------------------------------

if max(indices) >= 65536:
    raise RuntimeError("Radio unexpectedly requires 32-bit indices")


def make_index_buffer(idx):
    raw = struct.pack("<" + "H"*len(idx), *idx)

    out = bytearray()

    out += struct.pack("<i", 0)          # b32Bit = false
    out += struct.pack("<ii", 1, len(raw))
    out += raw
    out += struct.pack("<i", 0)          # bShouldExpandTo32Bit

    return out


main_index = make_index_buffer(indices)

# Reversed winding variant.
reversed_indices = []

for i in range(0, len(indices), 3):
    a, b, c = indices[i:i+3]
    reversed_indices.extend((a, c, b))

reversed_index = make_index_buffer(reversed_indices)

# Depth-only can safely use the same geometry for this prototype.
depth_index = make_index_buffer(indices)
reversed_depth_index = make_index_buffer(reversed_indices)


# ----------------------------------------------------------------------
# Write outputs.
# ----------------------------------------------------------------------

outputs = {
    "position.bin": position,
    "vertex.bin": vertex,
    "index.bin": main_index,
    "reversed_index.bin": reversed_index,
    "depth_index.bin": depth_index,
    "reversed_depth_index.bin": reversed_depth_index,
}

for name, blob in outputs.items():
    (OUT / name).write_bytes(blob)


# Expected serialized sizes.
expected_position = 16 + N * 12
expected_vertex = 34 + N * 16
expected_index = 16 + len(indices) * 2

assert len(position) == expected_position
assert len(vertex) == expected_vertex
assert len(main_index) == expected_index

print("UE4 buffer encoding successful")
print()
print(f"vertices:          {N}")
print(f"triangles:         {len(indices)//3}")
print(f"indices:           {len(indices)}")
print(f"max vertex index:  {max(indices)}")
print()
print(f"position buffer:   {len(position):,} bytes")
print(f"vertex/UV buffer:  {len(vertex):,} bytes")
print(f"main index buffer: {len(main_index):,} bytes")
print(f"reversed index:    {len(reversed_index):,} bytes")
print(f"depth index:       {len(depth_index):,} bytes")
print(f"rev depth index:   {len(reversed_depth_index):,} bytes")
print()
print("Generated:", OUT)

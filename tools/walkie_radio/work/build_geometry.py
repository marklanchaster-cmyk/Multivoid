#!/usr/bin/env python3

from pathlib import Path
import json
import math

ROOT = Path(__file__).resolve().parents[3]
OBJ = ROOT / "tools/walkie_radio/source/walkie_radio.obj"
OUT = ROOT / "tools/walkie_radio/work/radio_geometry.json"


def parse_idx(text, count):
    i = int(text)
    return i - 1 if i > 0 else count + i


positions = []
uvs = []
normals = []
faces = []

for raw in OBJ.read_text().splitlines():
    line = raw.strip()

    if line.startswith("v "):
        _, x, y, z = line.split()[:4]
        positions.append((float(x), float(y), float(z)))

    elif line.startswith("vt "):
        p = line.split()
        uvs.append((float(p[1]), float(p[2])))

    elif line.startswith("vn "):
        _, x, y, z = line.split()[:4]
        normals.append((float(x), float(y), float(z)))

    elif line.startswith("f "):
        verts = []

        for tok in line.split()[1:]:
            p = tok.split("/")

            vi = parse_idx(p[0], len(positions))
            ti = parse_idx(p[1], len(uvs)) if len(p) > 1 and p[1] else -1
            ni = parse_idx(p[2], len(normals)) if len(p) > 2 and p[2] else -1

            verts.append((vi, ti, ni))

        if len(verts) != 3:
            raise RuntimeError(f"Non-triangle survived export: {line}")

        faces.append(tuple(verts))


# UE needs one vertex whenever position / normal / UV identity differs.
vertex_map = {}
vertices = []
indices = []


def normalize(v):
    x, y, z = v
    n = math.sqrt(x*x + y*y + z*z)

    if n < 1e-12:
        return (0.0, 0.0, 1.0)

    return (x/n, y/n, z/n)


for face in faces:
    # Blender -> UE mirrors Y. Reflection changes handedness, so reverse
    # triangle winding at the same time.
    for vi, ti, ni in (face[0], face[2], face[1]):

        key = (vi, ti, ni)

        idx = vertex_map.get(key)

        if idx is None:
            px, py, pz = positions[vi]

            # Blender meters -> Unreal centimeters, with Y mirror.
            pos = (
                px * 100.0,
                -py * 100.0,
                pz * 100.0,
            )

            if ni >= 0:
                nx, ny, nz = normals[ni]
                normal = normalize((nx, -ny, nz))
            else:
                normal = (0.0, 0.0, 1.0)

            if ti >= 0:
                u, v = uvs[ti]

                # OBJ/Blender UV origin differs from cooked UE texture space.
                uv0 = (u, 1.0 - v)
            else:
                uv0 = (0.0, 0.0)

            # Template has 2 UV channels.
            # For prototype, duplicate UV0 into UV1.
            uv1 = uv0

            idx = len(vertices)
            vertex_map[key] = idx

            vertices.append({
                "position": pos,
                "normal": normal,
                "uv0": uv0,
                "uv1": uv1,
            })

        indices.append(idx)


if not vertices:
    raise RuntimeError("No vertices produced")


xs = [v["position"][0] for v in vertices]
ys = [v["position"][1] for v in vertices]
zs = [v["position"][2] for v in vertices]

mins = (min(xs), min(ys), min(zs))
maxs = (max(xs), max(ys), max(zs))

origin = tuple((a+b) * 0.5 for a, b in zip(mins, maxs))
extent = tuple((b-a) * 0.5 for a, b in zip(mins, maxs))

radius = 0.0
for v in vertices:
    x, y, z = v["position"]

    dx = x - origin[0]
    dy = y - origin[1]
    dz = z - origin[2]

    radius = max(radius, math.sqrt(dx*dx + dy*dy + dz*dz))


data = {
    "source_positions": len(positions),
    "source_uvs": len(uvs),
    "source_normals": len(normals),
    "triangles": len(faces),
    "vertices": vertices,
    "indices": indices,
    "bounds": {
        "origin": origin,
        "extent": extent,
        "sphere_radius": radius,
    },
}

OUT.write_text(json.dumps(data, indent=2))


print("Geometry conversion successful")
print(f"source positions: {len(positions)}")
print(f"source normals:   {len(normals)}")
print(f"source UVs:       {len(uvs)}")
print(f"triangles:        {len(faces)}")
print(f"UE vertices:      {len(vertices)}")
print(f"indices:          {len(indices)}")
print()
print("UE bounds:")
print(f"  min:    {mins}")
print(f"  max:    {maxs}")
print(f"  origin: {origin}")
print(f"  extent: {extent}")
print(f"  radius: {radius:.3f} cm")
print()
print("Wrote:", OUT)

#!/usr/bin/env python3

from pathlib import Path
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]

sys.path.insert(0, str(ROOT / "tools" / "client_model"))
import ue_pkg

WORK = ROOT / "tools/walkie_radio/work"
TEMPLATE = WORK / "citizen_template"
GEN = WORK / "generated"

UA_IN = TEMPLATE / "citizenradio_reference.uasset"
UX_IN = TEMPLATE / "citizenradio_reference.uexp"

UA_OUT = GEN / "citizenradio_reference.uasset"
UX_OUT = GEN / "citizenradio_reference.uexp"

LOD_IN_START = 40161
LOD_IN_END = 80145

ua = bytearray(UA_IN.read_bytes())
ux = UX_IN.read_bytes()

lod = (GEN / "lod0.bin").read_bytes()
geom = json.loads((WORK / "radio_geometry.json").read_text())

old_lod_size = LOD_IN_END - LOD_IN_START
delta = len(lod) - old_lod_size

print("LOD splice:")
print(f"  old size: {old_lod_size:,}")
print(f"  new size: {len(lod):,}")
print(f"  delta:    {delta:+,}")


# ----------------------------------------------------------------------
# Read package-summary fields.
# ----------------------------------------------------------------------

summary = ue_pkg.parse_summary(bytes(ua))

kv = {
    t[1]: t[3]
    for t in summary.tok
    if t[1] is not None and t[3] is not None
}

header_size = kv["totalHeaderSize"]
export_count = kv["exportCount"]
export_offset = kv["exportOffset"]
depends_offset = kv["dependsOffset"]
old_bulk_start = next(
    t[3]
    for t in summary.tok
    if t[1] == "bulkDataStartOffset"
)

if export_count != 3:
    raise RuntimeError(f"Expected 3 exports, got {export_count}")

export_record_size = (depends_offset - export_offset) // export_count

if export_record_size != 104:
    raise RuntimeError(
        f"Unexpected FObjectExport size: {export_record_size}"
    )

print()
print("Package:")
print(f"  header size:       {header_size}")
print(f"  export table:      {export_offset}")
print(f"  export record size:{export_record_size}")
print(f"  old bulk start:    {old_bulk_start}")


# ----------------------------------------------------------------------
# Verify FObjectExport serial fields.
#
# UE4.27 FObjectExport:
#
# +28  int64 SerialSize
# +36  int64 SerialOffset
# ----------------------------------------------------------------------

exports = []

for i in range(export_count):
    rec = export_offset + i * export_record_size

    serial_size = struct.unpack_from("<q", ua, rec + 28)[0]
    serial_offset = struct.unpack_from("<q", ua, rec + 36)[0]

    exports.append((serial_size, serial_offset))

    print(
        f"  export {i}: "
        f"offset={serial_offset} "
        f"size={serial_size}"
    )

expected = [
    (38629, 2579),
    (773, 41208),
    (40894, 41981),
]

if exports != expected:
    raise RuntimeError(
        "Export table does not match the package we probed.\n"
        f"Expected: {expected}\n"
        f"Got:      {exports}"
    )

mesh_size, mesh_abs_start = exports[2]

mesh_local_start = mesh_abs_start - header_size
mesh_local_end = mesh_local_start + mesh_size

if mesh_local_end != len(ux) - 4:
    raise RuntimeError(
        "StaticMesh is not immediately before the expected 4-byte "
        ".uexp trailer"
    )

print()
print("StaticMesh:")
print(f"  local range: {mesh_local_start} -> {mesh_local_end}")
print(f"  trailer:     {len(ux) - mesh_local_end} bytes")


# ----------------------------------------------------------------------
# Locate the original FBoxSphereBounds.
#
# These are the exact float32 values read by the parser from this
# template. Restrict the search to the StaticMesh tail after the LOD.
# ----------------------------------------------------------------------

# Exact cooked tail layout verified from citizenradio_reference.
expected_tail_prefix = bytes((
    0x01,              # NumInlinedLODs
    0x01, 0x00,        # distance-field strip flags
    0x00, 0x00, 0x00, 0x00,  # bValid = false
))

actual_tail_prefix = ux[
    LOD_IN_END : LOD_IN_END + len(expected_tail_prefix)
]

if actual_tail_prefix != expected_tail_prefix:
    raise RuntimeError(
        "Unexpected StaticMesh post-LOD layout.\n"
        f"Expected: {expected_tail_prefix.hex()}\n"
        f"Got:      {actual_tail_prefix.hex()}"
    )

# Bounds begin AFTER NumInlinedLODs + strip flags + the 4-byte UE bool.
old_bounds_pos = LOD_IN_END + 7

print()
print("Bounds:")
print(f"  old position: {old_bounds_pos}")


# ----------------------------------------------------------------------
# Splice the new LOD.
# ----------------------------------------------------------------------

new_ux = bytearray(
    ux[:LOD_IN_START]
    + lod
    + ux[LOD_IN_END:]
)

new_bounds_pos = old_bounds_pos + delta

b = geom["bounds"]

new_bounds = (
    *b["origin"],
    *b["extent"],
    b["sphere_radius"],
)

struct.pack_into(
    "<7f",
    new_ux,
    new_bounds_pos,
    *new_bounds,
)

print(f"  new position: {new_bounds_pos}")
print(f"  origin:       {tuple(b['origin'])}")
print(f"  extent:       {tuple(b['extent'])}")
print(f"  radius:       {b['sphere_radius']:.3f} cm")


# ----------------------------------------------------------------------
# Patch StaticMesh export size.
# ----------------------------------------------------------------------

mesh_rec = export_offset + 2 * export_record_size

new_mesh_size = mesh_size + delta

struct.pack_into(
    "<q",
    ua,
    mesh_rec + 28,
    new_mesh_size,
)


# ----------------------------------------------------------------------
# Patch FPackageFileSummary.BulkDataStartOffset.
#
# Find its exact byte offset from the parsed summary token stream.
# ----------------------------------------------------------------------

cursor = 0
bulk_field_offset = None

for token in summary.tok:
    kind, key, raw, value = token

    if key == "bulkDataStartOffset":
        bulk_field_offset = cursor
        break

    cursor += len(raw)

if bulk_field_offset is None:
    raise RuntimeError("bulkDataStartOffset field not found")

new_bulk_start = old_bulk_start + delta

struct.pack_into(
    "<q",
    ua,
    bulk_field_offset,
    new_bulk_start,
)


# ----------------------------------------------------------------------
# Cross-check all absolute/local boundaries.
# ----------------------------------------------------------------------

new_mesh_abs_end = mesh_abs_start + new_mesh_size
new_mesh_local_end = mesh_local_start + new_mesh_size

if new_mesh_abs_end != new_bulk_start:
    raise RuntimeError(
        "StaticMesh export end != new BulkDataStartOffset"
    )

if new_mesh_local_end != len(new_ux) - 4:
    raise RuntimeError(
        "StaticMesh no longer ends immediately before .uexp trailer"
    )

if len(new_ux) != len(ux) + delta:
    raise RuntimeError("Unexpected .uexp size after splice")


# Reparse modified .uasset summary as a sanity check.
check = ue_pkg.parse_summary(bytes(ua))

new_bulk_check = next(
    t[3]
    for t in check.tok
    if t[1] == "bulkDataStartOffset"
)

if new_bulk_check != new_bulk_start:
    raise RuntimeError("Modified summary did not reparse correctly")


UA_OUT.write_bytes(ua)
UX_OUT.write_bytes(new_ux)


print()
print("Patched package:")
print(f"  StaticMesh size:       {mesh_size:,} -> {new_mesh_size:,}")
print(f"  BulkDataStartOffset:   {old_bulk_start:,} -> {new_bulk_start:,}")
print(f"  .uexp size:            {len(ux):,} -> {len(new_ux):,}")
print(f"  StaticMesh abs end:    {new_mesh_abs_end:,}")
print()
print("Consistency checks: PASS")
print()
print("Wrote:")
print(" ", UA_OUT)
print(" ", UX_OUT)

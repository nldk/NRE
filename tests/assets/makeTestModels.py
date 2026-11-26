#!/usr/bin/env python3
"""Generates the small test models used by the visual tests (run from anywhere; writes next to this script).

  alphaQuad.gltf       2 m double-sided quad, base texture with alpha 0 in a 4x4 checker, alphaMode MASK, cutoff 0.5
  alphaQuad.png        the checker texture (opaque orange / transparent cells)
  materialSpheres.gltf two white spheres side by side: roughness 0.1 (x = -0.7) and 0.9 (x = +0.7), metallic 0
  noUv.obj             unit cube with no vt and no vn
  normalQuads.gltf     five 0.5 m quads in a row, constant normal maps: flat, tilted 45 deg toward +X, -X, +Y, -Y
  normalQuads.png      their normal map (one column per quad)
  metalQuads.gltf      three quads: metallic 0.5 from a texture, 0.5 from the factor, 1 from a texture
  metalQuads.png       their metallic-roughness map

Needs numpy and Pillow. glTF buffers are embedded as base64 data URIs, so each .gltf is self-contained.
"""
import base64
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image

OUT = Path(__file__).resolve().parent


def data_uri(raw: bytes) -> str:
    return "data:application/octet-stream;base64," + base64.b64encode(raw).decode("ascii")


def pack(blobs):
    """Concatenate blobs (4-byte aligned) -> (bytes, [(offset, length)])."""
    out = bytearray()
    spans = []
    for b in blobs:
        while len(out) % 4:
            out.append(0)
        spans.append((len(out), len(b)))
        out += b
    while len(out) % 4:
        out.append(0)
    return bytes(out), spans


def mesh_buffers(pos, nrm, uv, idx):
    pos = np.asarray(pos, dtype="<f4")
    nrm = np.asarray(nrm, dtype="<f4")
    uv = np.asarray(uv, dtype="<f4")
    idx = np.asarray(idx, dtype="<u2")
    data, spans = pack([pos.tobytes(), nrm.tobytes(), uv.tobytes(), idx.tobytes()])
    views = [
        {"buffer": 0, "byteOffset": spans[0][0], "byteLength": spans[0][1], "target": 34962},
        {"buffer": 0, "byteOffset": spans[1][0], "byteLength": spans[1][1], "target": 34962},
        {"buffer": 0, "byteOffset": spans[2][0], "byteLength": spans[2][1], "target": 34962},
        {"buffer": 0, "byteOffset": spans[3][0], "byteLength": spans[3][1], "target": 34963},
    ]
    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3",
         "min": pos.min(axis=0).tolist(), "max": pos.max(axis=0).tolist()},
        {"bufferView": 1, "componentType": 5126, "count": len(nrm), "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": len(uv), "type": "VEC2"},
        {"bufferView": 3, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
    ]
    return data, views, accessors


def uv_sphere(radius, rings=24, segments=48):
    pos, nrm, uv = [], [], []
    for r in range(rings + 1):
        v = r / rings
        phi = v * math.pi
        for s in range(segments + 1):
            u = s / segments
            theta = u * 2 * math.pi
            n = (math.sin(phi) * math.cos(theta), math.cos(phi), math.sin(phi) * math.sin(theta))
            nrm.append(n)
            pos.append(tuple(radius * c for c in n))
            uv.append((u, v))
    idx = []
    stride = segments + 1
    for r in range(rings):
        for s in range(segments):
            a = r * stride + s
            b = a + stride
            # counter-clockwise seen from outside
            idx += [a, a + 1, b, a + 1, b + 1, b]
    return pos, nrm, uv, idx


def write_alpha_quad():
    # 4x4 checker of 16 px cells: odd cells fully transparent (alpha 0), even cells opaque orange.
    cell = 16
    img = np.zeros((4 * cell, 4 * cell, 4), dtype=np.uint8)
    for cy in range(4):
        for cx in range(4):
            opaque = (cx + cy) % 2 == 0
            img[cy * cell:(cy + 1) * cell, cx * cell:(cx + 1) * cell] = (230, 120, 30, 255 if opaque else 0)
    Image.fromarray(img, "RGBA").save(OUT / "alphaQuad.png")

    pos = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    nrm = [(0, 0, 1)] * 4
    uv = [(0, 1), (1, 1), (1, 0), (0, 0)]  # glTF uv origin is top-left
    idx = [0, 1, 2, 0, 2, 3]
    data, views, accessors = mesh_buffers(pos, nrm, uv, idx)
    gltf = {
        "asset": {"version": "2.0", "generator": "makeTestModels.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "alphaQuad"}],
        "meshes": [{"name": "alphaQuad", "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0, "mode": 4}]}],
        "materials": [{"name": "alphaMask", "alphaMode": "MASK", "alphaCutoff": 0.5, "doubleSided": True,
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0, "roughnessFactor": 0.8}}],
        "textures": [{"sampler": 0, "source": 0}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "images": [{"uri": "alphaQuad.png"}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(data), "uri": data_uri(data)}],
    }
    (OUT / "alphaQuad.gltf").write_text(json.dumps(gltf, indent=1))


def write_material_spheres():
    pos, nrm, uv, idx = uv_sphere(0.5)
    data, views, accessors = mesh_buffers(pos, nrm, uv, idx)

    def material(name, rough):
        return {"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0.0, "roughnessFactor": rough}}

    prim = lambda m: {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": m, "mode": 4}
    gltf = {
        "asset": {"version": "2.0", "generator": "makeTestModels.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1]}],
        "nodes": [{"mesh": 0, "name": "smooth", "translation": [-0.7, 0, 0]},
                  {"mesh": 1, "name": "rough", "translation": [0.7, 0, 0]}],
        "meshes": [{"name": "smooth", "primitives": [prim(0)]}, {"name": "rough", "primitives": [prim(1)]}],
        "materials": [material("smooth", 0.1), material("rough", 0.9)],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(data), "uri": data_uri(data)}],
    }
    (OUT / "materialSpheres.gltf").write_text(json.dumps(gltf, indent=1))


def write_no_uv_obj():
    # Unit cube centred on the origin; positions and faces only (no vt, no vn).
    v = [(-.5, -.5, -.5), (.5, -.5, -.5), (.5, .5, -.5), (-.5, .5, -.5),
         (-.5, -.5, .5), (.5, -.5, .5), (.5, .5, .5), (-.5, .5, .5)]
    quads = [(1, 4, 3, 2), (5, 6, 7, 8), (1, 2, 6, 5), (4, 8, 7, 3), (1, 5, 8, 4), (2, 3, 7, 6)]  # outward CCW
    lines = ["# unit cube, positions only (no vt, no vn)", "o noUvCube"]
    lines += [f"v {x} {y} {z}" for x, y, z in v]
    for a, b, c, d in quads:
        lines.append(f"f {a} {b} {c}")
        lines.append(f"f {a} {c} {d}")
    (OUT / "noUv.obj").write_text("\n".join(lines) + "\n")


def write_quad_row(name, quads, materials, images):
    """A row of 0.5 m quads facing +Z, 0.6 m apart and centred on x = 0, one primitive each.

    quads: [(material index, (u0, v0, u1, v1))]; v0 is the quad's top edge (glTF's UV origin is top-left).
    images: PNG file names; texture i uses image i with a nearest/clamp sampler.
    """
    blobs, prims = [], []
    for i, (material, (u0, v0, u1, v1)) in enumerate(quads):
        x = (i - (len(quads) - 1) / 2) * 0.6
        pos = np.asarray([(x - .25, -.25, 0), (x + .25, -.25, 0), (x + .25, .25, 0), (x - .25, .25, 0)], dtype="<f4")
        nrm = np.asarray([(0, 0, 1)] * 4, dtype="<f4")
        uv = np.asarray([(u0, v1), (u1, v1), (u1, v0), (u0, v0)], dtype="<f4")
        idx = np.asarray([0, 1, 2, 0, 2, 3], dtype="<u2")
        first = len(blobs)
        blobs += [pos.tobytes(), nrm.tobytes(), uv.tobytes(), idx.tobytes()]
        prims.append((first, material, pos))
    data, spans = pack(blobs)
    views, accessors, primitives = [], [], []
    for first, material, pos in prims:
        for k, (kind, count, comp) in enumerate([("VEC3", 4, 5126), ("VEC3", 4, 5126), ("VEC2", 4, 5126), ("SCALAR", 6, 5123)]):
            off, length = spans[first + k]
            views.append({"buffer": 0, "byteOffset": off, "byteLength": length, "target": 34963 if k == 3 else 34962})
            acc = {"bufferView": first + k, "componentType": comp, "count": count, "type": kind}
            if k == 0:
                acc["min"], acc["max"] = pos.min(axis=0).tolist(), pos.max(axis=0).tolist()
            accessors.append(acc)
        primitives.append({"attributes": {"POSITION": first, "NORMAL": first + 1, "TEXCOORD_0": first + 2},
                           "indices": first + 3, "material": material, "mode": 4})
    gltf = {
        "asset": {"version": "2.0", "generator": "makeTestModels.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": primitives}],
        "materials": materials,
        "textures": [{"sampler": 0, "source": i} for i in range(len(images))],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "images": [{"uri": image} for image in images],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(data), "uri": data_uri(data)}],
    }
    (OUT / f"{name}.gltf").write_text(json.dumps(gltf, indent=1))


def column_image(path, colors, cell=8):
    """One cell-wide column of constant colour per entry, cell px high."""
    img = np.zeros((cell, cell * len(colors), 4), dtype=np.uint8)
    for i, c in enumerate(colors):
        img[:, i * cell:(i + 1) * cell] = (*c, 255)
    Image.fromarray(img, "RGBA").save(OUT / path)


def column_uv(i, count):
    """UV rect inside column i of a column_image, inset so filtering never reaches the next column."""
    return ((i + 0.25) / count, 0.25, (i + 0.75) / count, 0.75)


def write_normal_quads():
    # Constant tangent-space normals (glTF: +X right = +u, +Y up = -v, +Z out): flat, then tilted 45 degrees
    # toward +X, -X, +Y, -Y. White, non-metal, roughness 1, so only the normal differs.
    normals = [(128, 128, 255), (200, 128, 200), (56, 128, 200), (128, 200, 200), (128, 56, 200)]
    column_image("normalQuads.png", normals)
    material = {"name": "normalMapped", "normalTexture": {"index": 0},
                "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 1.0}}
    write_quad_row("normalQuads", [(0, column_uv(i, 5)) for i in range(5)], [material], ["normalQuads.png"])


def write_metal_quads():
    # Metallic-roughness textures (g = roughness, b = metallic, read as linear):
    #   left:   texture b = 128 (metallic 0.502 when read linear, 0.216 if wrongly decoded as sRGB), factor 1
    #   centre: no texture, metallicFactor 0.502: must match the left one
    #   right:  texture b = 255, factor 1: fully metal, so no diffuse
    column_image("metalQuads.png", [(0, 255, 128), (0, 255, 255)])
    white = [1, 1, 1, 1]
    materials = [
        {"name": "halfMetalTexture", "pbrMetallicRoughness": {"baseColorFactor": white, "metallicFactor": 1.0,
                                                               "roughnessFactor": 1.0, "metallicRoughnessTexture": {"index": 0}}},
        {"name": "halfMetalFactor", "pbrMetallicRoughness": {"baseColorFactor": white, "metallicFactor": 128 / 255,
                                                              "roughnessFactor": 1.0}},
        {"name": "fullMetalTexture", "pbrMetallicRoughness": {"baseColorFactor": white, "metallicFactor": 1.0,
                                                               "roughnessFactor": 1.0, "metallicRoughnessTexture": {"index": 0}}},
    ]
    quads = [(0, column_uv(0, 2)), (1, column_uv(0, 2)), (2, column_uv(1, 2))]
    write_quad_row("metalQuads", quads, materials, ["metalQuads.png"])


if __name__ == "__main__":
    write_alpha_quad()
    write_material_spheres()
    write_no_uv_obj()
    write_normal_quads()
    write_metal_quads()
    print("wrote models to", OUT)

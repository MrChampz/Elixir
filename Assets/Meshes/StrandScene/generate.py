"""Build the visible reference objects from particlez (without collision behavior)."""
import base64
import json
import math
import struct
import sys
from pathlib import Path

vertices, normals, indices = [], [], []


def surface(point, normal, center):
    vertices.append(tuple(point[i] + center[i] for i in range(3)))
    normals.append(normal)


def grid(rows, columns, sample, center):
    start = len(vertices)
    for row in range(rows + 1):
        for column in range(columns + 1):
            point, normal = sample(row / rows, column / columns)
            surface(point, normal, center)
    for row in range(rows):
        for column in range(columns):
            a = start + row * (columns + 1) + column
            indices.extend((a, a + 1, a + columns + 1, a + 1, a + columns + 2, a + columns + 1))


def sphere(center, radius, rows=16, columns=24):
    def sample(v, u):
        phi, theta = v * math.pi, u * math.tau
        normal = (math.sin(phi) * math.cos(theta), math.cos(phi), math.sin(phi) * math.sin(theta))
        return tuple(radius * x for x in normal), normal
    grid(rows, columns, sample, center)


def rounded_box(center, half, radius, subdivisions=12):
    for axis in range(3):
        for sign in (-1, 1):
            def sample(v, u, axis=axis, sign=sign):
                # Orient each face so the grid indices wind outward.
                other = ((axis + 1) % 3, (axis + 2) % 3)
                point = [0.0] * 3
                point[axis] = sign * half[axis]
                point[other[0]] = (u * 2 - 1) * half[other[0]] * sign
                point[other[1]] = (v * 2 - 1) * half[other[1]]
                inner = [max(-half[i] + radius, min(half[i] - radius, point[i])) for i in range(3)]
                delta = [point[i] - inner[i] for i in range(3)]
                length = math.sqrt(sum(x * x for x in delta))
                normal = tuple(x / length for x in delta)
                return tuple(inner[i] + radius * normal[i] for i in range(3)), normal
            grid(subdivisions, subdivisions, sample, center)


sphere((0.62, 0.05, 0), 0.24)
rounded_box((-0.6, 0.1, -0.05), (0.2, 0.2, 0.2), 0.05)
sphere((0, 0.6, -0.1), 0.16)
rounded_box((-0.05, -0.62, 0), (0.34, 0.11, 0.24), 0.03)

if "--wall" in sys.argv:
    vertices.clear()
    normals.clear()
    indices.clear()
    rounded_box((0, 0, -0.12), (1.2, 1.0, 0.04), 0.01)

if "--poles" in sys.argv:
    vertices.clear()
    normals.clear()
    indices.clear()
    for x in (-1.05, 0.0, 1.05):
        rounded_box((x, -.075, 0), (.045, .775, .045), .012, 4)
        rounded_box((x, .61, 0), (.065, .035, .24), .01, 4)
        for z in (-.16, 0.0, .16):
            rounded_box((x, .655, z), (.025, .03, .025), .008, 4)
            sphere((x, .69, z), .027, 8, 12)

chunks = [struct.pack('<' + 'f' * len(values) * 3, *(x for value in values for x in value)) for values in (vertices, normals)]
chunks.append(struct.pack('<' + 'I' * len(indices), *indices))
views, offset = [], 0
for index, chunk in enumerate(chunks):
    views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(chunk), 'target': 34963 if index == 2 else 34962})
    offset += len(chunk)
asset = {
    'asset': {'version': '2.0', 'generator': 'Elixir strand reference scene'},
    'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': 'Reference objects'}],
    'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}, 'indices': 2, 'material': 0}]}],
    'materials': [{'name': 'Reference blue', 'pbrMetallicRoughness': {'baseColorFactor': [0.12, 0.3, 0.48, 1], 'metallicFactor': 0.1, 'roughnessFactor': 0.42}}],
    'buffers': [{'byteLength': offset, 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(b''.join(chunks)).decode()}],
    'bufferViews': views,
    'accessors': [
        {'bufferView': 0, 'componentType': 5126, 'count': len(vertices), 'type': 'VEC3', 'min': [min(v[i] for v in vertices) for i in range(3)], 'max': [max(v[i] for v in vertices) for i in range(3)]},
        {'bufferView': 1, 'componentType': 5126, 'count': len(normals), 'type': 'VEC3'},
        {'bufferView': 2, 'componentType': 5125, 'count': len(indices), 'type': 'SCALAR'},
    ],
}
if "--poles" in sys.argv:
    asset['nodes'][0]['name'] = 'Utility poles'
    asset['materials'][0]['name'] = 'Pole wood'
    asset['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [.32, .2, .11, 1]
    asset['materials'][0]['pbrMetallicRoughness']['metallicFactor'] = 0
    asset['materials'][0]['pbrMetallicRoughness']['roughnessFactor'] = .75
filename = 'PowerPoles.gltf' if '--poles' in sys.argv else ('StrandWall.gltf' if '--wall' in sys.argv else 'StrandScene.gltf')
Path(__file__).with_name(filename).write_text(json.dumps(asset, separators=(',', ':')) + '\n')

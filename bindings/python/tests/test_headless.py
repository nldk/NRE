"""Headless tests: no window, no GPU work. Run from bindings/python with ``python3 -m unittest``.

Needs libnre_c 0.3 (set NRE_LIB_DIR, e.g. to <repo>/cmake-build-debug/bin). The error checks rely on the Debug
build's misuse checks; a Release library trusts its caller and may crash on them instead.
"""

from __future__ import annotations

import array
import ctypes
import os
import re
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import nre  # noqa: E402
from nre import _ffi  # noqa: E402

NRE_H = Path(__file__).resolve().parents[3] / "include" / "nre.h"


def setUpModule() -> None:
    # Errors before set_render_config() have no context, so the renderer treats them as Panic and aborts the
    # process. Configure (without init(), so no window) to get the Return policy. Never shut down: shutdown()
    # without init() isn't safe in the renderer.
    nre.set_render_config(64, 64, "nre headless test", error_policy=nre.RenderErrorPolicy.RETURN)


class VersionAndErrors(unittest.TestCase):
    def test_version_is_0_3(self) -> None:
        v = nre.get_version()
        self.assertEqual((v.major, v.minor), (0, 3))
        self.assertEqual((_ffi.ABI_MAJOR, _ffi.ABI_MINOR), (0, 3))
        if sys.platform != "win32":
            self.assertIn("0.3", _ffi.library_name())
        major, minor, _patch = v  # unpacks like a tuple
        self.assertEqual(major, 0)

    def test_error_code_to_string(self) -> None:
        self.assertEqual(nre.error_code_to_string(nre.ErrorCode.OK), "Ok")
        self.assertEqual(nre.error_code_to_string(nre.ErrorCode.FILE_NOT_FOUND), "FileNotFound")
        self.assertEqual(nre.error_code_to_string(nre.ErrorCode.NOT_INITIALIZED), "NotInitialized")
        self.assertEqual(nre.error_code_to_string(nre.ErrorCode.LIBRARY_LOAD_FAILED), "LibraryLoadFailed")
        self.assertEqual(nre.ErrorCode.INVALID_ARGUMENT, 30)
        self.assertEqual(nre.error_code_to_string(nre.ErrorCode.INVALID_ARGUMENT), "InvalidArgument")
        self.assertIs(nre.Error(30, "").code, nre.ErrorCode.INVALID_ARGUMENT)

    def test_load_rejects_other_versions(self) -> None:
        # A stale libnre_c 0.2 next to the 0.3 one must be refused by the version check. Load it in a child
        # process, so this process keeps only the 0.3 library.
        lib_dir = os.environ.get("NRE_LIB_DIR")
        old = Path(lib_dir) / "libnre_c.so.0.2" if lib_dir else None
        if sys.platform != "linux" or old is None or not old.exists():
            self.skipTest("no libnre_c.so.0.2 in $NRE_LIB_DIR")
        code = ("import sys; sys.path.insert(0, sys.argv[1]); from nre import _ffi\n"
                "try:\n    _ffi.load(sys.argv[2])\nexcept _ffi.LoadError as e:\n    print(e)\n    sys.exit(3)")
        child = subprocess.run([sys.executable, "-c", code, str(Path(__file__).resolve().parents[1]), str(old)],
                               capture_output=True, text=True, timeout=30)
        self.assertEqual(child.returncode, 3, child.stderr)
        self.assertIn("is libnre_c 0.2.", child.stdout)
        self.assertIn("target 0.3.x", child.stdout)

    def test_unknown_error_code_maps_to_unknown(self) -> None:
        self.assertIs(nre.ErrorCode(12345), nre.ErrorCode.UNKNOWN)
        e = nre.Error(12345, "from the future")
        self.assertIs(e.code, nre.ErrorCode.UNKNOWN)
        self.assertEqual(e.raw_code, 12345)

    def test_create_model_before_set_render_config_is_not_initialized(self) -> None:
        # The renderer only reports NotInitialized when there's no context, and without a context its error
        # policy is Panic: the process aborts instead of returning the code. So run it in a child process.
        code = ("import sys; sys.path.insert(0, sys.argv[1]); import nre; "
                "nre.create_model('missing.glb')")
        child = subprocess.run([sys.executable, "-c", code, str(Path(__file__).resolve().parents[1])],
                               capture_output=True, text=True, timeout=30)
        self.assertNotEqual(child.returncode, 0)
        self.assertIn("NotInitialized: createModel() was called before setRenderConfig()", child.stderr)

    def test_failed_call_raises_error_with_code_and_message(self) -> None:
        with self.assertRaises(nre.Error) as cm:
            nre.create_instance(nre.Model())  # the empty handle
        self.assertIs(cm.exception.code, nre.ErrorCode.INVALID_HANDLE)
        self.assertIsInstance(cm.exception.code, nre.ErrorCode)
        self.assertTrue(cm.exception.message)
        self.assertEqual(nre.get_last_error_message(), cm.exception.message)
        self.assertIn("INVALID_HANDLE", str(cm.exception))

    def test_set_render_config_twice_is_already_initialized(self) -> None:
        with self.assertRaises(nre.Error) as cm:
            nre.set_render_config(64, 64, "again")
        self.assertIs(cm.exception.code, nre.ErrorCode.ALREADY_INITIALIZED)
        self.assertTrue(cm.exception.message)


class Layouts(unittest.TestCase):
    def test_struct_sizes_match_nre_h(self) -> None:
        self.assertEqual(ctypes.sizeof(_ffi.NreVec2), 8)
        self.assertEqual(ctypes.sizeof(_ffi.NreVec3), 12)
        self.assertEqual(ctypes.sizeof(_ffi.NreQuat), 16)
        self.assertEqual(ctypes.sizeof(_ffi.NreMat4), 64)
        self.assertEqual(ctypes.sizeof(_ffi.NreTransform), 40)
        self.assertEqual(_ffi.NreTransform.position.offset, 0)
        self.assertEqual(_ffi.NreTransform.rotation.offset, 12)
        self.assertEqual(_ffi.NreTransform.scale.offset, 28)
        for h in _ffi.HANDLE_STRUCTS:
            self.assertEqual(ctypes.sizeof(h), 8, h.__name__)
            self.assertEqual(h.generation.offset, 4, h.__name__)
        self.assertEqual(ctypes.sizeof(_ffi.NreVertex), 32)
        self.assertEqual(_ffi.NreVertex.position.offset, 0)
        self.assertEqual(_ffi.NreVertex.normal.offset, 12)
        self.assertEqual(_ffi.NreVertex.uv.offset, 24)
        self.assertEqual(ctypes.sizeof(_ffi.NreRayHit), 24)
        self.assertEqual(_ffi.NreRayHit.instance.offset, 0)
        self.assertEqual(_ffi.NreRayHit.distance.offset, 8)
        self.assertEqual(_ffi.NreRayHit.point.offset, 12)

    def test_every_nre_h_function_is_bound(self) -> None:
        if not NRE_H.exists():
            self.skipTest(f"{NRE_H} not found")
        declared = set(re.findall(r"NRE_C_EXPORT\s+[\w\s\*]+?\b(nre\w+)\s*\(", NRE_H.read_text()))
        self.assertGreaterEqual(len(declared), 89)
        self.assertEqual(declared, set(_ffi.PROTOTYPES))
        for c_name in declared:
            snake = re.sub(r"(?<!^)(?=[A-Z])", "_", c_name[3:]).lower().replace("f_p_s", "fps")
            self.assertTrue(callable(getattr(nre, snake, None)), f"{c_name}: no nre.{snake}")
            self.assertIn(snake, nre.__all__)

    def test_enum_values_match_nre_h(self) -> None:
        if not NRE_H.exists():
            self.skipTest(f"{NRE_H} not found")
        values = {name: int(v, 0) for name, v in re.findall(r"\b(NRE_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)",
                                                             NRE_H.read_text())}
        self.assertEqual(values["NRE_ERROR_NOT_INITIALIZED"], nre.ErrorCode.NOT_INITIALIZED)
        self.assertEqual(values["NRE_ERROR_LIBRARY_LOAD_FAILED"], nre.ErrorCode.LIBRARY_LOAD_FAILED)
        self.assertEqual(values["NRE_SHADER_VERTEX"], nre.ShaderType.VERTEX)
        self.assertEqual(values["NRE_API_MODE_FUNCTION_DRAW"], nre.ApiMode.FUNCTION_DRAW)
        self.assertEqual(values["NRE_ERROR_POLICY_PANIC"], nre.RenderErrorPolicy.PANIC)
        self.assertEqual(values["NRE_CURSOR_LOCKED"], nre.CursorMode.LOCKED)
        self.assertEqual(values["NRE_MOUSE_BUTTON_MIDDLE"], nre.MouseButton.MIDDLE)
        lighting = {k[len("NRE_SCENE_LIGHTING_"):]: v for k, v in values.items()
                    if k.startswith("NRE_SCENE_LIGHTING_") and k != "NRE_SCENE_LIGHTING_MAX_ENUM"}
        self.assertEqual(lighting, {m.name: m.value for m in nre.SceneLighting})
        self.assertEqual(lighting, {"SMALL": 0, "LANDSCAPE": 1})
        keys = {k[len("NRE_KEY_"):]: v for k, v in values.items()
                if k.startswith("NRE_KEY_") and k != "NRE_KEY_MAX_ENUM"}
        digits = "ZERO ONE TWO THREE FOUR FIVE SIX SEVEN EIGHT NINE".split()
        expected = {(digits[int(k)] if k.isdigit() else k): v for k, v in keys.items()}
        self.assertEqual(expected, {k.name: k.value for k in nre.Key})
        codes = {k[len("NRE_ERROR_"):]: v for k, v in values.items()
                 if k.startswith("NRE_ERROR_") and not k.startswith("NRE_ERROR_POLICY_")
                 and k != "NRE_ERROR_CODE_MAX_ENUM"}
        codes["OK"] = values["NRE_OK"]
        self.assertEqual(codes, {c.name: c.value for c in nre.ErrorCode})


class Values(unittest.TestCase):
    def test_handles_compare_by_value_and_type(self) -> None:
        self.assertEqual(nre.Model(1, 2), nre.Model(1, 2))
        self.assertNotEqual(nre.Model(1, 2), nre.Model(1, 3))
        self.assertNotEqual(nre.Model(1, 2), nre.Instance(1, 2))
        self.assertEqual(len({nre.Model(1, 2), nre.Model(1, 2)}), 1)
        self.assertFalse(nre.Texture())
        self.assertTrue(nre.Texture().is_empty)
        self.assertEqual(nre.Cubemap.empty(), nre.Cubemap(0, 0))
        self.assertTrue(nre.Model(1, 0))

    def test_math_values(self) -> None:
        x, y, z = nre.Vec3(1, 2, 3)
        self.assertEqual((x, y, z), (1, 2, 3))
        self.assertEqual(nre.Vec3(1, 2, 3) + (1, 1, 1), nre.Vec3(2, 3, 4))
        self.assertEqual(2 * nre.Vec3(1, 2, 3), (2, 4, 6))
        self.assertEqual(nre.Quat(), (0, 0, 0, 1))
        t = nre.Transform()
        self.assertEqual((t.position, t.rotation, t.scale), ((0, 0, 0), (0, 0, 0, 1), (1, 1, 1)))
        self.assertEqual(nre.Mat4().at(3, 3), 1.0)
        self.assertEqual(nre.Mat4(range(16)).column(1), (4, 5, 6, 7))
        self.assertEqual(nre.Mat4(range(16)).row(1), (1, 5, 9, 13))

    def test_unknown_scene_lighting_is_an_error(self) -> None:
        self.assertIs(nre._scene_lighting(1), nre.SceneLighting.LANDSCAPE)
        with self.assertRaises(nre.Error) as cm:
            nre._scene_lighting(7)
        self.assertIs(cm.exception.code, nre.ErrorCode.UNKNOWN)

    def test_vertex_and_ray_hit_values(self) -> None:
        v = nre.Vertex((1, 2, 3))
        self.assertEqual((v.position, v.normal, v.uv), ((1, 2, 3), (0, 0, 0), (0, 0)))
        hit = nre.RayHit(nre.Instance(1, 1), 2.5, nre.Vec3(0, 0, 0.5))
        instance, distance, point = hit
        self.assertEqual((instance, distance, point.z), (nre.Instance(1, 1), 2.5, 0.5))

    def test_mesh_vertices_from_sequences(self) -> None:
        arr = nre._vertex_array([nre.Vertex((0, 0, 0)), ((1, 0, 0), (0, 0, 1), (1, 0))])
        self.assertEqual(len(arr), 2)
        self.assertEqual((arr[1].position.x, arr[1].normal.z, arr[1].uv.x), (1.0, 1.0, 1.0))
        self.assertEqual(len(nre._vertex_array(v for v in [nre.Vertex()])), 1)  # any iterable
        with self.assertRaises(TypeError):
            nre._vertex_array([(0, 0, 0)])  # a bare position isn't a Vertex

    def test_mesh_vertices_from_buffers_without_per_vertex_copies(self) -> None:
        c = (_ffi.NreVertex * 3)()
        self.assertIs(nre._vertex_array(c), c)
        floats = array.array("f", range(16))  # 2 packed vertices, shared (writable)
        arr = nre._vertex_array(floats)
        self.assertEqual(len(arr), 2)
        self.assertEqual(ctypes.addressof(arr), floats.buffer_info()[0])
        self.assertEqual((arr[1].position.x, arr[1].uv.y), (8.0, 15.0))
        self.assertEqual(len(nre._vertex_array(bytes(64))), 2)  # read-only: copied in one go
        with self.assertRaises(TypeError):
            nre._vertex_array(array.array("d", range(16)))  # float64
        with self.assertRaises(ValueError):
            nre._vertex_array(array.array("f", range(9)))  # not a whole number of vertices

    def test_mesh_indices(self) -> None:
        self.assertEqual(list(nre._index_array([0, 1, 2])), [0, 1, 2])
        self.assertEqual(list(nre._index_array(array.array("I", [3, 4]))), [3, 4])
        c = (ctypes.c_uint32 * 2)(5, 6)
        self.assertIs(nre._index_array(c), c)
        with self.assertRaises(ValueError):
            nre._index_array([-1])
        with self.assertRaises(TypeError):
            nre._index_array(array.array("q", [1]))  # 64-bit

    def test_vector_parameters_reject_wrong_shapes(self) -> None:
        with self.assertRaises(TypeError):
            nre.set_camera_position((1, 2))
        with self.assertRaises(TypeError):
            nre.set_camera_position(5)  # type: ignore[arg-type]


if __name__ == "__main__":
    unittest.main()

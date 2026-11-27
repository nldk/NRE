package nre;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.StructLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Locale;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BOOLEAN;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;

/**
 * The FFM glue: loads libnre_c, describes the nre.h structs as memory layouts and holds one downcall handle
 * per nre.h function. {@link NRE} is the public face; this class is package-private.
 */
@SuppressWarnings("restricted") // native access is the point; the JVM needs --enable-native-access
final class Native {
    private Native() {}

    /** The ABI these bindings target: nre.h of version 0.3.x. */
    static final int ABI_MAJOR = 0, ABI_MINOR = 3;

    // ---------------------------------------------------------------------------------------------
    // Layouts, matching nre.h. C enums are 32-bit ints (the *_MAX_ENUM values make sure of that).
    // ---------------------------------------------------------------------------------------------
    static final StructLayout VEC2 = MemoryLayout.structLayout(
            JAVA_FLOAT.withName("x"), JAVA_FLOAT.withName("y")).withName("NreVec2");
    static final StructLayout VEC3 = MemoryLayout.structLayout(
            JAVA_FLOAT.withName("x"), JAVA_FLOAT.withName("y"), JAVA_FLOAT.withName("z")).withName("NreVec3");
    static final StructLayout QUAT = MemoryLayout.structLayout(
            JAVA_FLOAT.withName("x"), JAVA_FLOAT.withName("y"), JAVA_FLOAT.withName("z"),
            JAVA_FLOAT.withName("w")).withName("NreQuat");
    static final StructLayout MAT4 = MemoryLayout.structLayout(
            MemoryLayout.sequenceLayout(16, JAVA_FLOAT).withName("m")).withName("NreMat4");
    static final StructLayout TRANSFORM = MemoryLayout.structLayout(
            VEC3.withName("position"), QUAT.withName("rotation"), VEC3.withName("scale")).withName("NreTransform");
    /** Every handle type (NreShader, NreModel, ...) has this layout. */
    static final StructLayout HANDLE = MemoryLayout.structLayout(
            JAVA_INT.withName("index"), JAVA_INT.withName("generation")).withName("NreHandle");

    static final StructLayout VERTEX = MemoryLayout.structLayout(
            VEC3.withName("position"), VEC3.withName("normal"), VEC2.withName("uv")).withName("NreVertex");
    static final StructLayout RAY_HIT = MemoryLayout.structLayout(
            HANDLE.withName("instance"), JAVA_FLOAT.withName("distance"), VEC3.withName("point")).withName("NreRayHit");

    static final long TRANSFORM_ROTATION = TRANSFORM.byteOffset(MemoryLayout.PathElement.groupElement("rotation"));
    static final long TRANSFORM_SCALE = TRANSFORM.byteOffset(MemoryLayout.PathElement.groupElement("scale"));
    static final long VERTEX_NORMAL = VERTEX.byteOffset(MemoryLayout.PathElement.groupElement("normal"));
    static final long VERTEX_UV = VERTEX.byteOffset(MemoryLayout.PathElement.groupElement("uv"));
    static final long RAY_HIT_DISTANCE = RAY_HIT.byteOffset(MemoryLayout.PathElement.groupElement("distance"));
    static final long RAY_HIT_POINT = RAY_HIT.byteOffset(MemoryLayout.PathElement.groupElement("point"));

    // ---------------------------------------------------------------------------------------------
    // Loading
    // ---------------------------------------------------------------------------------------------
    static final String LIBRARY_NAME = libraryName();
    private static final Linker LINKER = Linker.nativeLinker();
    private static final SymbolLookup LIB = load();

    private static String libraryName() {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        if (os.startsWith("windows")) return "nre_c.dll";
        if (os.startsWith("mac") || os.startsWith("darwin")) return "libnre_c." + ABI_MAJOR + "." + ABI_MINOR + ".dylib";
        return "libnre_c.so." + ABI_MAJOR + "." + ABI_MINOR;
    }

    private static SymbolLookup load() {
        String dir = System.getenv("NRE_LIB_DIR");
        try {
            if (dir != null && !dir.isEmpty()) {
                Path path = Path.of(dir).resolve(LIBRARY_NAME).toAbsolutePath();
                if (!Files.exists(path)) {
                    throw new UnsatisfiedLinkError("NRE: " + path + " not found (NRE_LIB_DIR=" + dir + ")");
                }
                return SymbolLookup.libraryLookup(path, Arena.global());
            }
            return SymbolLookup.libraryLookup(LIBRARY_NAME, Arena.global());
        } catch (IllegalArgumentException e) {
            UnsatisfiedLinkError error = new UnsatisfiedLinkError("NRE: could not load " + LIBRARY_NAME
                    + (dir == null || dir.isEmpty()
                       ? " from the system library path; set NRE_LIB_DIR to the directory that has it"
                       : " from NRE_LIB_DIR=" + dir) + ": " + e.getMessage());
            error.initCause(e);
            throw error;
        }
    }

    private static MethodHandle fn(String name, FunctionDescriptor descriptor) {
        MemorySegment symbol = LIB.find(name).orElseThrow(
                () -> new UnsatisfiedLinkError("NRE: " + LIBRARY_NAME + " has no symbol " + name));
        return LINKER.downcallHandle(symbol, descriptor);
    }

    // Shorthands for the descriptors below.
    private static final MemoryLayout I = JAVA_INT, F = JAVA_FLOAT, B = JAVA_BOOLEAN, P = ADDRESS, H = HANDLE, V3 = VEC3;

    /** Returns NreErrorCode. */
    private static FunctionDescriptor err(MemoryLayout... args) { return FunctionDescriptor.of(JAVA_INT, args); }

    // Errors and version
    static final MethodHandle GET_LAST_ERROR_MESSAGE = fn("nreGetLastErrorMessage", FunctionDescriptor.of(P));
    static final MethodHandle ERROR_CODE_TO_STRING = fn("nreErrorCodeToString", FunctionDescriptor.of(P, I));
    static final MethodHandle GET_VERSION = fn("nreGetVersion", FunctionDescriptor.ofVoid(P, P, P));

    static final Version VERSION = checkVersion();

    private static Version checkVersion() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment major = a.allocate(JAVA_INT), minor = a.allocate(JAVA_INT), patch = a.allocate(JAVA_INT);
            GET_VERSION.invokeExact(major, minor, patch);
            Version v = new Version(major.get(JAVA_INT, 0), minor.get(JAVA_INT, 0), patch.get(JAVA_INT, 0));
            if (v.major() != ABI_MAJOR || v.minor() != ABI_MINOR) {
                throw new UnsatisfiedLinkError("NRE: loaded " + LIBRARY_NAME + " is version " + v
                        + ", but these bindings are for " + ABI_MAJOR + "." + ABI_MINOR + ".x");
            }
            return v;
        } catch (RuntimeException | Error e) {
            throw e;
        } catch (Throwable t) {
            throw new IllegalStateException(t);
        }
    }

    // Setup and frames
    static final MethodHandle SET_RENDER_CONFIG = fn("nreSetRenderConfig", err(I, I, I, B, I, I, P, B, B));
    static final MethodHandle INIT = fn("nreInit", err());
    static final MethodHandle SHUTDOWN = fn("nreShutdown", err());
    static final MethodHandle RENDER_FRAME = fn("nreRenderFrame", err());
    static final MethodHandle END_FRAME = fn("nreEndFrame", err());
    static final MethodHandle PRESENT_FRAME = fn("nrePresentFrame", err());
    static final MethodHandle SUBMIT_FRAME = fn("nreSubmitFrame", err());
    static final MethodHandle WINDOW_SHOULD_CLOSE = fn("nreWindowShouldClose", err(P));
    static final MethodHandle POLL_EVENTS = fn("nrePollEvents", err());

    // Shaders and pipelines
    static final MethodHandle CREATE_SHADER = fn("nreCreateShader", err(P, I, P));
    static final MethodHandle DESTROY_SHADER = fn("nreDestroyShader", err(H));
    static final MethodHandle CREATE_PIPELINE = fn("nreCreatePipeline", err(H, H, P));
    static final MethodHandle DESTROY_PIPELINE = fn("nreDestroyPipeline", err(H));
    static final MethodHandle BIND_PIPELINE = fn("nreBindPipeline", err(H));

    // Models and instances
    static final MethodHandle CREATE_MODEL = fn("nreCreateModel", err(P, P));
    static final MethodHandle CREATE_MESH_MODEL = fn("nreCreateMeshModel", err(P, I, P, I, H, P));
    static final MethodHandle DESTROY_MODEL = fn("nreDestroyModel", err(H));
    static final MethodHandle CREATE_INSTANCE = fn("nreCreateInstance", err(H, P));
    static final MethodHandle DESTROY_INSTANCE = fn("nreDestroyInstance", err(H));
    static final MethodHandle SET_INSTANCE_TRANSFORM = fn("nreSetInstanceTransform", err(H, P));
    static final MethodHandle GET_INSTANCE_TRANSFORM = fn("nreGetInstanceTransform", err(H, P));
    static final MethodHandle GET_INSTANCE_MATRIX = fn("nreGetInstanceMatrix", err(H, P));
    static final MethodHandle TRANSLATE_INSTANCE = fn("nreTranslateInstance", err(H, V3));
    static final MethodHandle ROTATE_INSTANCE = fn("nreRotateInstance", err(H, F, V3));
    static final MethodHandle SCALE_INSTANCE = fn("nreScaleInstance", err(H, V3));
    static final MethodHandle SET_VISIBILITY = fn("nreSetVisibility", err(H, B));
    static final MethodHandle SET_CASTS_SHADOWS = fn("nreSetCastsShadows", err(H, B));
    static final MethodHandle DRAW_MODEL = fn("nreDrawModel", err(H, P));
    static final MethodHandle DRAW_INSTANCE = fn("nreDrawInstance", err(H));

    // Scenes
    static final MethodHandle CREATE_SCENE = fn("nreCreateScene", err(P));
    static final MethodHandle DESTROY_SCENE = fn("nreDestroyScene", err(H));
    static final MethodHandle BIND_SCENE = fn("nreBindScene", err(H));
    static final MethodHandle GET_BOUND_SCENE = fn("nreGetBoundScene", err(P));
    static final MethodHandle ADD_INSTANCE_TO_SCENE = fn("nreAddInstanceToScene", err(H));
    static final MethodHandle REMOVE_INSTANCE_FROM_SCENE = fn("nreRemoveInstanceFromScene", err(H));

    // Textures and materials
    static final MethodHandle CREATE_TEXTURE = fn("nreCreateTexture", err(P, B, P));
    static final MethodHandle DESTROY_TEXTURE = fn("nreDestroyTexture", err(H));
    static final MethodHandle CREATE_MATERIAL = fn("nreCreateMaterial", err(H, H, H, P));
    static final MethodHandle DESTROY_MATERIAL = fn("nreDestroyMaterial", err(H));
    static final MethodHandle SET_MATERIAL_BASE_TEXTURE = fn("nreSetMaterialBaseTexture", err(H, H));
    static final MethodHandle GET_MODEL_MATERIALS = fn("nreGetModelMaterials", err(H, P, P));
    static final MethodHandle SET_MODEL_MATERIAL = fn("nreSetModelMaterial", err(H, H));
    static final MethodHandle REPLACE_MODEL_MATERIAL = fn("nreReplaceModelMaterial", err(H, I, H));

    // Lights and the sun
    static final MethodHandle CREATE_DIRECTIONAL_LIGHT = fn("nreCreateDirectionalLight", err(V3, V3, F, P));
    static final MethodHandle CREATE_POINT_LIGHT = fn("nreCreatePointLight", err(V3, V3, F, P));
    static final MethodHandle DESTROY_LIGHT = fn("nreDestroyLight", err(H));
    static final MethodHandle ADD_LIGHT_TO_SCENE = fn("nreAddLightToScene", err(H));
    static final MethodHandle REMOVE_LIGHT_FROM_SCENE = fn("nreRemoveLightFromScene", err(H));
    static final MethodHandle SET_LIGHT_POSITION = fn("nreSetLightPosition", err(H, V3));
    static final MethodHandle SET_LIGHT_DIRECTION = fn("nreSetLightDirection", err(H, V3));
    static final MethodHandle SET_LIGHT_COLOR = fn("nreSetLightColor", err(H, V3, F));
    static final MethodHandle SET_SUN = fn("nreSetSun", err(V3, V3, F));
    static final MethodHandle SET_SUN_DIRECTION = fn("nreSetSunDirection", err(V3));
    static final MethodHandle GET_SUN_DIRECTION = fn("nreGetSunDirection", err(P));

    // Scene lighting, shadows, haze, exposure and sky colour
    static final MethodHandle SET_SCENE_LIGHTING = fn("nreSetSceneLighting", err(I));
    static final MethodHandle GET_SCENE_LIGHTING = fn("nreGetSceneLighting", err(P));
    static final MethodHandle SET_SHADOW_DISTANCE = fn("nreSetShadowDistance", err(F));
    static final MethodHandle SET_HAZE = fn("nreSetHaze", err(F, F));
    static final MethodHandle SET_EXPOSURE = fn("nreSetExposure", err(F));
    static final MethodHandle SET_SKY_COLOR = fn("nreSetSkyColor", err(V3));

    // Cubemaps and the skybox
    static final MethodHandle CREATE_CUBEMAP = fn("nreCreateCubemap", err(P, P, P, P, P, P, P));
    static final MethodHandle DESTROY_CUBEMAP = fn("nreDestroyCubemap", err(H));
    static final MethodHandle SET_SKYBOX = fn("nreSetSkybox", err(H));

    // Camera
    static final MethodHandle SET_CAMERA_POSITION = fn("nreSetCameraPosition", err(V3));
    static final MethodHandle GET_CAMERA_POSITION = fn("nreGetCameraPosition", err(P));
    static final MethodHandle MOVE_CAMERA = fn("nreMoveCamera", err(V3));
    static final MethodHandle MOVE_CAMERA_RELATIVE = fn("nreMoveCameraRelative", err(V3));
    static final MethodHandle SET_CAMERA_ROTATION = fn("nreSetCameraRotation", err(F, F));
    static final MethodHandle ROTATE_CAMERA = fn("nreRotateCamera", err(F, F));
    static final MethodHandle CAMERA_LOOK_AT = fn("nreCameraLookAt", err(V3));
    static final MethodHandle GET_CAMERA_FORWARD = fn("nreGetCameraForward", err(P));
    static final MethodHandle GET_VIEW_MATRIX = fn("nreGetViewMatrix", err(P));
    static final MethodHandle GET_PROJECTION_MATRIX = fn("nreGetProjectionMatrix", err(P));
    static final MethodHandle PICK_INSTANCE = fn("nrePickInstance", err(VEC2, P, P));

    // Timing
    static final MethodHandle GET_FPS = fn("nreGetFPS", err(P));
    static final MethodHandle GET_DELTA_TIME = fn("nreGetDeltaTime", err(P));

    // Input
    static final MethodHandle IS_MOUSE_BUTTON_DOWN = fn("nreIsMouseButtonDown", err(I, P));
    static final MethodHandle IS_MOUSE_BUTTON_PRESSED = fn("nreIsMouseButtonPressed", err(I, P));
    static final MethodHandle IS_MOUSE_BUTTON_RELEASED = fn("nreIsMouseButtonReleased", err(I, P));
    static final MethodHandle GET_MOUSE_DELTA = fn("nreGetMouseDelta", err(P));
    static final MethodHandle GET_SCROLL_DELTA = fn("nreGetScrollDelta", err(P));
    static final MethodHandle GET_MOUSE_POSITION = fn("nreGetMousePosition", err(P));
    static final MethodHandle SET_CURSOR_MODE = fn("nreSetCursorMode", err(I));
    static final MethodHandle IS_KEY_DOWN = fn("nreIsKeyDown", err(I, P));
    static final MethodHandle IS_KEY_PRESSED = fn("nreIsKeyPressed", err(I, P));
    static final MethodHandle IS_KEY_RELEASED = fn("nreIsKeyReleased", err(I, P));

    // ---------------------------------------------------------------------------------------------
    // Struct conversion
    // ---------------------------------------------------------------------------------------------
    static MemorySegment handle(Arena a, Handle h) {
        MemorySegment s = a.allocate(HANDLE);
        s.set(JAVA_INT, 0, h.index());
        s.set(JAVA_INT, 4, h.generation());
        return s;
    }

    static int index(MemorySegment s, long offset) { return s.get(JAVA_INT, offset); }
    static int generation(MemorySegment s, long offset) { return s.get(JAVA_INT, offset + 4); }

    static MemorySegment vec3(Arena a, Vec3 v) {
        MemorySegment s = a.allocate(VEC3);
        writeVec3(s, 0, v);
        return s;
    }

    static void writeVec3(MemorySegment s, long offset, Vec3 v) {
        s.set(JAVA_FLOAT, offset, v.x());
        s.set(JAVA_FLOAT, offset + 4, v.y());
        s.set(JAVA_FLOAT, offset + 8, v.z());
    }

    static MemorySegment vec2(Arena a, Vec2 v) {
        MemorySegment s = a.allocate(VEC2);
        s.set(JAVA_FLOAT, 0, v.x());
        s.set(JAVA_FLOAT, 4, v.y());
        return s;
    }

    static Vec2 readVec2(MemorySegment s) { return new Vec2(s.get(JAVA_FLOAT, 0), s.get(JAVA_FLOAT, 4)); }

    static Vec3 readVec3(MemorySegment s, long offset) {
        return new Vec3(s.get(JAVA_FLOAT, offset), s.get(JAVA_FLOAT, offset + 4), s.get(JAVA_FLOAT, offset + 8));
    }

    static MemorySegment transform(Arena a, Transform t) {
        MemorySegment s = a.allocate(TRANSFORM);
        writeVec3(s, 0, t.position());
        Quat q = t.rotation();
        s.set(JAVA_FLOAT, TRANSFORM_ROTATION, q.x());
        s.set(JAVA_FLOAT, TRANSFORM_ROTATION + 4, q.y());
        s.set(JAVA_FLOAT, TRANSFORM_ROTATION + 8, q.z());
        s.set(JAVA_FLOAT, TRANSFORM_ROTATION + 12, q.w());
        writeVec3(s, TRANSFORM_SCALE, t.scale());
        return s;
    }

    static Transform readTransform(MemorySegment s) {
        long r = TRANSFORM_ROTATION;
        return new Transform(readVec3(s, 0),
                new Quat(s.get(JAVA_FLOAT, r), s.get(JAVA_FLOAT, r + 4), s.get(JAVA_FLOAT, r + 8), s.get(JAVA_FLOAT, r + 12)),
                readVec3(s, TRANSFORM_SCALE));
    }

    /** The vertices as one NreVertex array (one allocation for the lot); NULL for none. */
    static MemorySegment vertices(Arena a, List<Vertex> vertices) {
        if (vertices.isEmpty()) return MemorySegment.NULL;
        MemorySegment s = a.allocate(VERTEX, vertices.size());
        long offset = 0;
        for (Vertex v : vertices) {
            writeVec3(s, offset, v.position());
            writeVec3(s, offset + VERTEX_NORMAL, v.normal());
            s.set(JAVA_FLOAT, offset + VERTEX_UV, v.uv().x());
            s.set(JAVA_FLOAT, offset + VERTEX_UV + 4, v.uv().y());
            offset += VERTEX.byteSize();
        }
        return s;
    }

    static RayHit readRayHit(MemorySegment s) {
        return new RayHit(new Instance(index(s, 0), generation(s, 0)), s.get(JAVA_FLOAT, RAY_HIT_DISTANCE),
                readVec3(s, RAY_HIT_POINT));
    }

    static Mat4 readMat4(MemorySegment s) { return new Mat4(s.toArray(JAVA_FLOAT)); }

    /** A NUL-terminated C string the library owns, copied into a Java String. */
    static String string(MemorySegment cString) {
        return cString.equals(MemorySegment.NULL) ? "" : cString.reinterpret(Long.MAX_VALUE).getString(0);
    }
}

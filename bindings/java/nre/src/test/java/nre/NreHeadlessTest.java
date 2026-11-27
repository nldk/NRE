package nre;

import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.function.ToIntFunction;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static org.junit.jupiter.api.Assertions.*;
import static org.junit.jupiter.api.Assumptions.assumeTrue;

/** Checks that need no window or GPU: loading, errors, struct layouts, and that the bindings match nre.h. */
class NreHeadlessTest {
    // Before setRenderConfig there's no renderer context, and the library treats every error as Panic (abort).
    // setRenderConfig only stores the config (no window), so after it errors are returned (policy RETURN).
    @BeforeAll
    static void configureWithoutInit() {
        NRE.setRenderConfig(64, 64, "headless test", RenderMode.RENDER_3D, ApiMode.SCENE, RenderErrorPolicy.RETURN);
    }

    @Test
    void versionIsTheOneTheseBindingsTarget() {
        Version v = NRE.getVersion();
        System.out.println("libnre_c version " + v);
        assertEquals(0, v.major());
        assertEquals(3, v.minor());
        assertEquals(v, Native.VERSION);
    }

    /**
     * The version check rejects a library of another minor version: a copy of libnre_c 0.2 under the 0.3 name
     * (a symlink in a temp dir) must fail to load, in a fresh JVM. Skipped when there's no libnre_c.so.0.2 next to
     * the 0.3 one, or not on Linux.
     */
    @Test
    void versionCheckRejectsOtherMinorVersions(@TempDir Path tmp) throws Exception {
        String libDir = System.getenv("NRE_LIB_DIR");
        assumeTrue(libDir != null && !libDir.isEmpty(), "NRE_LIB_DIR not set");
        assumeTrue(Native.LIBRARY_NAME.startsWith("libnre_c.so."), "Linux only");
        Path old = Path.of(libDir, "libnre_c.so.0.2");
        assumeTrue(Files.exists(old), "no " + old);
        Path link = Files.createSymbolicLink(tmp.resolve(Native.LIBRARY_NAME), old.toRealPath());
        String output;
        try {
            output = probe(VersionProbe.class, tmp.toString());
        } finally {
            Files.delete(link);
        }
        System.out.println("0.2 under the 0.3 name: " + output.strip());
        assertTrue(output.contains("REJECTED") && output.contains("version 0.2"), output);
    }

    /** Runs main of probe in a fresh JVM with NRE_LIB_DIR=libDir, and returns its output. */
    private static String probe(Class<?> probe, String libDir) throws Exception {
        String java = ProcessHandle.current().info().command().orElseThrow();
        ProcessBuilder pb = new ProcessBuilder(java, "--enable-native-access=ALL-UNNAMED",
                "-cp", System.getProperty("java.class.path"), probe.getName())
                .redirectErrorStream(true);
        pb.environment().put("NRE_LIB_DIR", libDir);
        Process p = pb.start();
        String output = new String(p.getInputStream().readAllBytes());
        p.waitFor();
        return output;
    }

    @Test
    void errorCodeToString() {
        assertEquals("Ok", NRE.errorCodeToString(ErrorCode.OK));
        assertEquals("FileNotFound", NRE.errorCodeToString(ErrorCode.FILE_NOT_FOUND));
        assertEquals("NotInitialized", NRE.errorCodeToString(ErrorCode.NOT_INITIALIZED));
        assertEquals("InvalidArgument", NRE.errorCodeToString(ErrorCode.INVALID_ARGUMENT));
        assertEquals(30, ErrorCode.INVALID_ARGUMENT.value());
        for (ErrorCode code : ErrorCode.values()) {
            String s = NRE.errorCodeToString(code);
            assertFalse(s.isEmpty(), code.name());
            assertFalse(s.contains("Not in enum"), code + " is unknown to the library: " + s);
        }
    }

    @Test
    void unknownErrorValuesMapToUnknown() {
        assertEquals(ErrorCode.UNKNOWN, ErrorCode.fromValue(12345));
        assertEquals(ErrorCode.UNKNOWN, ErrorCode.fromValue(-1));
        assertEquals(ErrorCode.LIBRARY_LOAD_FAILED, ErrorCode.fromValue(29));
        assertEquals(ErrorCode.INVALID_ARGUMENT, ErrorCode.fromValue(30));
        assertEquals(ErrorCode.UNKNOWN, ErrorCode.fromValue(31));
        NreException e = new NreException(12345, "from the future");
        assertEquals(ErrorCode.UNKNOWN, e.code());
        assertEquals(12345, e.rawCode());
    }

    @Test
    void missingFileThrowsFileNotFound() {
        NreException e = assertThrows(NreException.class, () -> NRE.createModel("/nonexistent/model.glb"));
        System.out.println("createModel(missing file): " + e.getMessage());
        assertEquals(ErrorCode.FILE_NOT_FOUND, e.code());
        assertFalse(e.nreMessage().isEmpty());
        assertEquals(e.nreMessage(), NRE.getLastErrorMessage());
    }

    @Test
    void emptyHandleThrowsInvalidHandle() {
        NreException e = assertThrows(NreException.class, () -> NRE.getInstanceTransform(Instance.NONE));
        System.out.println("getInstanceTransform(Instance.NONE): " + e.getMessage());
        assertEquals(ErrorCode.INVALID_HANDLE, e.code());
        assertFalse(e.nreMessage().isEmpty());
        assertEquals(e.nreMessage(), NRE.getLastErrorMessage());
    }

    @Test
    void configuringTwiceThrowsAlreadyInitialized() {
        NreException e = assertThrows(NreException.class, () -> NRE.setRenderConfig(64, 64, "again"));
        System.out.println("setRenderConfig twice: " + e.getMessage());
        assertEquals(ErrorCode.ALREADY_INITIALIZED, e.code());
        assertFalse(e.nreMessage().isEmpty());
    }

    /**
     * createModel before init, in a fresh JVM (this one has already called setRenderConfig). nre.h says it returns
     * NRE_ERROR_NOT_INITIALIZED; librenderer (0.2.0 and 0.3.0) reports NotInitialized but then aborts, because without a
     * renderer context its error policy is Panic. Accept either: a thrown NOT_INITIALIZED with a message, or the
     * abort with the library's NotInitialized message.
     */
    @Test
    void createModelBeforeInitReportsNotInitialized() throws Exception {
        String java = ProcessHandle.current().info().command().orElseThrow();
        Process p = new ProcessBuilder(java, "--enable-native-access=ALL-UNNAMED",
                "-cp", System.getProperty("java.class.path"), NotInitializedProbe.class.getName(), policeGlb())
                .redirectErrorStream(true)
                .start();
        String output = new String(p.getInputStream().readAllBytes());
        int exit = p.waitFor();
        System.out.println("probe exit " + exit + ", output:\n" + output.strip());
        if (output.contains("CODE=NOT_INITIALIZED")) {
            assertFalse(output.contains("MSG=\n"), "empty message");
        } else {
            assertNotEquals(0, exit);
            assertTrue(output.contains("[NRE] NotInitialized: createModel() was called before"), output);
        }
    }

    private static String policeGlb() {
        return Path.of(System.getProperty("nre.header")).getParent().getParent()
                .resolve("example/assets/police.glb").toString();
    }

    @Test
    void layoutsMatchNreH() {
        assertEquals(8, Native.VEC2.byteSize());
        assertEquals(12, Native.VEC3.byteSize());
        assertEquals(16, Native.QUAT.byteSize());
        assertEquals(64, Native.MAT4.byteSize());
        assertEquals(40, Native.TRANSFORM.byteSize());
        assertEquals(12, Native.TRANSFORM_ROTATION);
        assertEquals(28, Native.TRANSFORM_SCALE);
        assertEquals(8, Native.HANDLE.byteSize());
        assertEquals(32, Native.VERTEX.byteSize());
        assertEquals(12, Native.VERTEX_NORMAL);
        assertEquals(24, Native.VERTEX_UV);
        assertEquals(24, Native.RAY_HIT.byteSize());
        assertEquals(8, Native.RAY_HIT_DISTANCE);
        assertEquals(12, Native.RAY_HIT_POINT);
    }

    @Test
    void sceneLightingValues() {
        assertEquals(0, SceneLighting.SMALL.value());
        assertEquals(1, SceneLighting.LANDSCAPE.value());
        assertEquals(SceneLighting.LANDSCAPE, SceneLighting.fromValue(1));
        assertThrows(IllegalArgumentException.class, () -> SceneLighting.fromValue(2));
        assertThrows(IllegalArgumentException.class, () -> SceneLighting.fromValue(-1));
    }

    @Test
    void vertexAndRayHitStructs() {
        try (Arena a = Arena.ofConfined()) {
            var s = Native.vertices(a, List.of(
                    new Vertex(new Vec3(1, 2, 3), new Vec3(4, 5, 6), new Vec2(7, 8)), new Vertex(new Vec3(9, 10, 11))));
            assertEquals(64, s.byteSize());
            float[] f = s.toArray(JAVA_FLOAT);
            assertArrayEquals(new float[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0}, f);
            assertEquals(MemorySegment.NULL, Native.vertices(a, List.of()));

            var hit = a.allocate(Native.RAY_HIT);
            hit.set(JAVA_INT, 0, 5);
            hit.set(JAVA_INT, 4, 2);
            hit.set(JAVA_FLOAT, 8, 2.5f);
            hit.set(JAVA_FLOAT, 12, 1);
            hit.set(JAVA_FLOAT, 16, 2);
            hit.set(JAVA_FLOAT, 20, 3);
            assertEquals(new RayHit(new Instance(5, 2), 2.5f, new Vec3(1, 2, 3)), Native.readRayHit(hit));
        }
    }

    @Test
    void mathTypes() {
        assertEquals(new Quat(0, 0, 0, 1), Transform.IDENTITY.rotation());
        assertEquals(new Vec3(5, 7, 9), new Vec3(1, 2, 3).add(new Vec3(4, 5, 6)));
        assertEquals(new Vec3(2, 4, 6), new Vec3(1, 2, 3).scale(2));
        float[] m = Mat4.IDENTITY.toArray();
        m[0] = 42;
        assertEquals(1, Mat4.IDENTITY.get(0, 0), "Mat4 must copy its array out");
        assertTrue(Model.NONE.isNone());
    }

    // ---------------------------------------------------------------------------------------------
    // Against the header itself (path from the build: -Dnre.header)
    // ---------------------------------------------------------------------------------------------

    private static String header() throws IOException {
        String path = System.getProperty("nre.header");
        assertNotNull(path, "run through Gradle, which sets -Dnre.header");
        return Files.readString(Path.of(path));
    }

    /** NAME = value pairs of the C enum constants that start with prefix (without the MAX_ENUM ones). */
    private static TreeMap<String, Integer> cEnum(String header, String prefix) {
        TreeMap<String, Integer> result = new TreeMap<>();
        Matcher m = Pattern.compile("\\b(" + prefix + "\\w*)\\s*=\\s*(0x[0-9A-Fa-f]+|\\d+)").matcher(header);
        while (m.find()) {
            if (m.group(1).endsWith("MAX_ENUM")) continue;
            result.put(m.group(1), Integer.decode(m.group(2)));
        }
        return result;
    }

    private static <E extends Enum<E>> Set<Integer> values(E[] constants, ToIntFunction<E> value) {
        return Arrays.stream(constants).map(value::applyAsInt).collect(Collectors.toCollection(TreeSet::new));
    }

    @Test
    void errorCodesMatchHeaderByName() throws IOException {
        TreeMap<String, Integer> c = cEnum(header(), "NRE_(?:OK|ERROR_)");
        c.keySet().removeIf(k -> k.startsWith("NRE_ERROR_POLICY_"));
        TreeMap<String, Integer> java = new TreeMap<>();
        for (ErrorCode e : ErrorCode.values()) java.put(e == ErrorCode.OK ? "NRE_OK" : "NRE_ERROR_" + e.name(), e.value());
        assertEquals(c, java);
    }

    @Test
    void otherEnumsMatchHeader() throws IOException {
        String h = header();
        assertEquals(new TreeSet<>(cEnum(h, "NRE_SHADER_").values()), values(ShaderType.values(), ShaderType::value));
        assertEquals(new TreeSet<>(cEnum(h, "NRE_RENDER_").values()), values(RenderMode.values(), RenderMode::value));
        assertEquals(new TreeSet<>(cEnum(h, "NRE_API_MODE_").values()), values(ApiMode.values(), ApiMode::value));
        assertEquals(new TreeSet<>(cEnum(h, "NRE_ERROR_POLICY_").values()),
                values(RenderErrorPolicy.values(), RenderErrorPolicy::value));
        assertEquals(new TreeSet<>(cEnum(h, "NRE_MOUSE_BUTTON_").values()), values(MouseButton.values(), MouseButton::value));
        assertEquals(new TreeSet<>(cEnum(h, "NRE_CURSOR_").values()), values(CursorMode.values(), CursorMode::value));
        TreeMap<String, Integer> lighting = new TreeMap<>();
        for (SceneLighting l : SceneLighting.values()) lighting.put("NRE_SCENE_LIGHTING_" + l.name(), l.value());
        assertEquals(cEnum(h, "NRE_SCENE_LIGHTING_"), lighting);
        TreeMap<String, Integer> keys = cEnum(h, "NRE_KEY_");
        assertEquals(keys.size(), Key.values().length);
        assertEquals(new TreeSet<>(keys.values()), values(Key.values(), Key::value));
    }

    @Test
    void everyHeaderFunctionHasAMethod() throws IOException {
        Matcher m = Pattern.compile("NRE_C_EXPORT[^;(]*?\\bnre(\\w+)\\s*\\(").matcher(header());
        Set<String> methods = Arrays.stream(NRE.class.getDeclaredMethods())
                .filter(x -> Modifier.isPublic(x.getModifiers()))
                .map(Method::getName)
                .collect(Collectors.toSet());
        int count = 0;
        TreeSet<String> missing = new TreeSet<>();
        while (m.find()) {
            count++;
            String name = Character.toLowerCase(m.group(1).charAt(0)) + m.group(1).substring(1);
            if (!methods.contains(name)) missing.add(name);
        }
        System.out.println(count + " functions in nre.h");
        assertTrue(count >= 89, "parsed too few functions: " + count);
        assertEquals(Set.of(), missing);
    }
}

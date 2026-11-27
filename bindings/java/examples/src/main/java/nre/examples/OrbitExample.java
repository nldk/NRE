package nre.examples;

import nre.ApiMode;
import nre.Cubemap;
import nre.Instance;
import nre.Model;
import nre.MouseButton;
import nre.Pipeline;
import nre.RenderErrorPolicy;
import nre.RenderMode;
import nre.Shader;
import nre.ShaderType;
import nre.Vec2;
import nre.Vec3;

import java.net.URISyntaxException;
import java.nio.file.Files;
import java.nio.file.Path;

import static nre.NRE.*;

/**
 * Port of example/main.cpp: a police car on the plains skybox, slowly rotating, with an orbit camera
 * (drag with the left mouse button to orbit, scroll to zoom). Pass "scene" as the first argument for SceneMode;
 * the default is FunctionDrawMode, like main.cpp.
 *
 * <p>Run: {@code ./gradlew :examples:runJava} (needs NRE_LIB_DIR, see the README). Shaders come from
 * $NRE_SHADER_DIR, by default &lt;repo&gt;/cmake-build-debug/example/shaders.
 */
public final class OrbitExample {
    /** The camera lives in the app, not in the bindings: it only uses NRE's input and camera functions. */
    static final class OrbitCamera {
        static final float ROTATE_SPEED = 0.01f;
        static final float ZOOM_STEP = 0.9f;
        static final float MAX_PITCH = 1.55f;

        Vec3 target = Vec3.ZERO;
        float distance = 3.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;

        void update() {
            if (isMouseButtonDown(MouseButton.LEFT)) {
                Vec2 d = getMouseDelta();
                yaw -= d.x() * ROTATE_SPEED;
                pitch = Math.clamp(pitch + d.y() * ROTATE_SPEED, -MAX_PITCH, MAX_PITCH);
            }

            distance = Math.max(distance * (float) Math.pow(ZOOM_STEP, getScrollDelta()), 0.1f);

            Vec3 offset = new Vec3((float) (Math.cos(pitch) * Math.sin(yaw)), (float) Math.sin(pitch),
                    (float) (Math.cos(pitch) * Math.cos(yaw)));
            setCameraPosition(target.add(offset.scale(distance)));
            cameraLookAt(target);
        }
    }

    public static void main(String[] args) {
        ApiMode apiMode = args.length > 0 && args[0].equals("scene") ? ApiMode.SCENE : ApiMode.FUNCTION_DRAW;
        Path repo = findRepo();
        Path assets = repo.resolve("example/assets");
        String shaderEnv = System.getenv("NRE_SHADER_DIR");
        Path shaders = shaderEnv != null && !shaderEnv.isEmpty()
                       ? Path.of(shaderEnv) : repo.resolve("cmake-build-debug/example/shaders");

        setRenderConfig(500, 500, "test", RenderMode.RENDER_3D, apiMode, RenderErrorPolicy.IGNORE);
        init();
        Model model = createModel(assets.resolve("police.glb").toString());
        Instance car = createInstance(model);
        Shader vertex = createShader(shaders.resolve("vertex.vert.spv").toString(), ShaderType.VERTEX);
        Shader frag = createShader(shaders.resolve("fragment.frag.spv").toString(), ShaderType.FRAGMENT);
        Pipeline pipeline = createPipeline(vertex, frag);
        bindPipeline(pipeline);
        createScene();
        addInstanceToScene(car);
        translateInstance(car, new Vec3(0.0f, 0.0f, -1.0f));
        Path sky = assets.resolve("skyboxes/plains");
        Cubemap skybox = createCubemap(sky.resolve("right.png").toString(), sky.resolve("left.png").toString(),
                sky.resolve("top.png").toString(), sky.resolve("bottom.png").toString(),
                sky.resolve("front.png").toString(), sky.resolve("back.png").toString());
        setSkybox(skybox);

        OrbitCamera camera = new OrbitCamera();
        camera.target = getInstanceTransform(car).position();
        while (!windowShouldClose()) {
            pollEvents();
            camera.update();
            if (apiMode == ApiMode.SCENE) {
                renderFrame();
            } else {
                renderFrame();
                drawInstance(car);
                endFrame();
            }

            rotateInstance(car, 0.0001f, new Vec3(0.0f, 1.0f, 0.0f));
        }
        shutdown();
    }

    /** The repository root, found from where this class was loaded (not the working directory). */
    static Path findRepo() {
        try {
            Path dir = Path.of(OrbitExample.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            for (; dir != null; dir = dir.getParent()) {
                if (Files.isRegularFile(dir.resolve("include/nre.h"))) return dir;
            }
        } catch (URISyntaxException | SecurityException e) {
            throw new IllegalStateException(e);
        }
        throw new IllegalStateException("can't find the NRE repository above " + OrbitExample.class
                .getProtectionDomain().getCodeSource().getLocation());
    }
}

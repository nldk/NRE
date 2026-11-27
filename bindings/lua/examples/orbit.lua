-- Port of example/main.cpp: a police car on the plains skybox, with an orbit camera
-- (drag with the left mouse button to orbit, scroll to zoom) and the car slowly rotating.
--
--   NRE_LIB_DIR=<repo>/cmake-build-debug/bin luajit examples/orbit.lua
--
-- Shaders come from $NRE_SHADER_DIR, default <repo>/cmake-build-debug/example/shaders.

local here = debug.getinfo(1, "S").source:match("^@(.*)[/\\]") or "."
package.path = here .. "/../?.lua;" .. package.path

local nre = require("nre")

local REPO = here .. "/../../.."
local ASSET_DIR = REPO .. "/example/assets"
local SHADER_DIR = os.getenv("NRE_SHADER_DIR") or (REPO .. "/cmake-build-debug/example/shaders")

-- The orbit camera belongs to the app, not the bindings.
local OrbitCamera = {}
OrbitCamera.__index = OrbitCamera

local ROTATE_SPEED = 0.01
local ZOOM_STEP = 0.9
local MAX_PITCH = 1.55

function OrbitCamera.new()
    return setmetatable({ target = nre.vec3(0, 0, 0), distance = 3.0, yaw = 0.0, pitch = 0.0 }, OrbitCamera)
end

function OrbitCamera:update()
    if nre.is_mouse_button_down(nre.MouseButton.LEFT) then
        local d = nre.get_mouse_delta()
        self.yaw = self.yaw - d.x * ROTATE_SPEED
        self.pitch = math.max(-MAX_PITCH, math.min(MAX_PITCH, self.pitch + d.y * ROTATE_SPEED))
    end

    self.distance = math.max(self.distance * ZOOM_STEP ^ nre.get_scroll_delta(), 0.1)

    local cp = math.cos(self.pitch)
    local offset = nre.vec3(cp * math.sin(self.yaw), math.sin(self.pitch), cp * math.cos(self.yaw))
    nre.set_camera_position(self.target + offset * self.distance)
    nre.camera_look_at(self.target)
end

local API_MODE = nre.ApiMode.FUNCTION_DRAW -- or nre.ApiMode.SCENE

nre.set_render_config(500, 500, "test", nre.RenderMode.RENDER_3D, API_MODE, nre.RenderErrorPolicy.IGNORE)
nre.init()
local model = nre.create_model(ASSET_DIR .. "/police.glb")
local car = nre.create_instance(model)
local vertex = nre.create_shader(SHADER_DIR .. "/vertex.vert.spv", nre.ShaderType.VERTEX)
local frag = nre.create_shader(SHADER_DIR .. "/fragment.frag.spv", nre.ShaderType.FRAGMENT)
local pipeline = nre.create_pipeline(vertex, frag)
nre.bind_pipeline(pipeline)
local scene = nre.create_scene() -- the first scene is bound automatically
nre.add_instance_to_scene(car)
nre.translate_instance(car, { 0, 0, -1 })
local dir = ASSET_DIR .. "/skyboxes/plains/"
local sky = nre.create_cubemap(dir .. "right.png", dir .. "left.png", dir .. "top.png",
    dir .. "bottom.png", dir .. "front.png", dir .. "back.png")
nre.set_skybox(sky)

local camera = OrbitCamera.new()
-- Copy the field: a struct field is a reference into its parent cdata, which the GC may free.
camera.target = nre.Vec3(nre.get_instance_transform(car).position)
while not nre.window_should_close() do
    nre.poll_events()
    camera:update()
    if API_MODE == nre.ApiMode.SCENE then
        nre.render_frame()
    elseif API_MODE == nre.ApiMode.FUNCTION_DRAW then
        nre.render_frame()
        nre.draw_instance(car)
        nre.end_frame()
    end

    nre.rotate_instance(car, 0.0001, { 0, 1, 0 })
end
nre.shutdown()

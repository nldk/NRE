-- Headless tests for the LuaJIT bindings: no window, no GPU work.
--   NRE_LIB_DIR=<repo>/cmake-build-debug/bin luajit tests/headless.lua
-- Exits non-zero if anything fails.

local here = debug.getinfo(1, "S").source:match("^@(.*)[/\\]") or "."
package.path = here .. "/../?.lua;" .. package.path

local ffi = require("ffi")
local nre = require("nre")

local failures, passed = 0, 0
local function test(name, fn)
    local ok, err = pcall(fn)
    if ok then
        passed = passed + 1
        print("ok    " .. name)
    else
        failures = failures + 1
        print("FAIL  " .. name .. ": " .. tostring(err))
    end
end

local function eq(a, b, what)
    if a ~= b then error(("%s: expected %s, got %s"):format(what or "value", tostring(b), tostring(a)), 2) end
end

-- Runs fn, which must raise an nre.Error with this code; returns the error.
local function expect_error(code, fn, ...)
    local ok, err = pcall(fn, ...)
    if ok then error("expected an error, but the call succeeded", 2) end
    if not nre.is_error(err) then error("expected an nre.Error, got: " .. tostring(err), 2) end
    if err.code ~= code then error(("expected code %d, got %s"):format(code, tostring(err)), 2) end
    return err
end

-- ---------------------------------------------------------------------------------------------------
test("version is 0.3.x", function()
    local major, minor, patch = nre.get_version()
    eq(major, 0, "major"); eq(minor, 3, "minor")
    assert(type(patch) == "number")
end)

test("struct sizes and layouts match nre.h", function()
    eq(ffi.sizeof("NreVec2"), 8, "Vec2")
    eq(ffi.sizeof("NreVec3"), 12, "Vec3")
    eq(ffi.sizeof("NreQuat"), 16, "Quat")
    eq(ffi.sizeof("NreMat4"), 64, "Mat4")
    eq(ffi.sizeof("NreTransform"), 40, "Transform")
    eq(ffi.offsetof("NreTransform", "position"), 0, "Transform.position")
    eq(ffi.offsetof("NreTransform", "rotation"), 12, "Transform.rotation")
    eq(ffi.offsetof("NreTransform", "scale"), 28, "Transform.scale")
    eq(ffi.sizeof("NreVertex"), 32, "Vertex")
    eq(ffi.offsetof("NreVertex", "position"), 0, "Vertex.position")
    eq(ffi.offsetof("NreVertex", "normal"), 12, "Vertex.normal")
    eq(ffi.offsetof("NreVertex", "uv"), 24, "Vertex.uv")
    eq(ffi.sizeof("NreRayHit"), 24, "RayHit")
    eq(ffi.offsetof("NreRayHit", "instance"), 0, "RayHit.instance")
    eq(ffi.offsetof("NreRayHit", "distance"), 8, "RayHit.distance")
    eq(ffi.offsetof("NreRayHit", "point"), 12, "RayHit.point")
    for _, h in ipairs({ "Shader", "Pipeline", "Model", "Instance", "Light", "Scene", "Texture", "Material", "Cubemap" }) do
        eq(ffi.sizeof("Nre" .. h), 8, h)
        eq(ffi.offsetof("Nre" .. h, "generation"), 4, h .. ".generation")
    end
    for _, e in ipairs({ "NreErrorCode", "NreShaderType", "NreRenderMode", "NreApiMode", "NreSceneLighting",
                         "NreRenderErrorPolicy", "NreMouseButton", "NreCursorMode", "NreKey" }) do
        eq(ffi.sizeof(e), 4, e)
    end
end)

test("enum tables", function()
    eq(nre.ErrorCode.OK, 0); eq(nre.ErrorCode.NOT_INITIALIZED, 9); eq(nre.ErrorCode.LIBRARY_LOAD_FAILED, 29)
    eq(nre.ErrorCode.INVALID_ARGUMENT, 30)
    eq(nre.SceneLighting.SMALL, 0); eq(nre.SceneLighting.LANDSCAPE, 1); eq(nre.SceneLighting.MAX_ENUM, nil)
    eq(nre.ShaderType.FRAGMENT, 0); eq(nre.ShaderType.VERTEX, 1)
    eq(nre.RenderMode.RENDER_3D, 0); eq(nre.ApiMode.FUNCTION_DRAW, 1); eq(nre.RenderErrorPolicy.PANIC, 2)
    eq(nre.MouseButton.MIDDLE, 2); eq(nre.CursorMode.LOCKED, 2)
    eq(nre.Key.W, 87); eq(nre.Key["0"], 48); eq(nre.Key.KP_EQUAL, 336); eq(nre.Key.MENU, 348)
    eq(nre.Key.MAX_ENUM, nil, "MAX_ENUM left out")
end)

test("error_code_to_string", function()
    eq(nre.error_code_to_string(nre.ErrorCode.OK), "Ok")
    eq(nre.error_code_to_string(nre.ErrorCode.NOT_INITIALIZED), "NotInitialized")
    eq(nre.error_code_to_string(nre.ErrorCode.FILE_NOT_FOUND), "FileNotFound")
    eq(nre.error_code_to_string(nre.ErrorCode.INVALID_ARGUMENT), "InvalidArgument")
    eq(nre.error_code_to_string(999), "Unknown")
    eq(nre.error_code_to_string(-1), "Unknown")
end)

test("unknown error codes raise safely", function()
    local err = expect_error(12345, nre.check, 12345)
    eq(err.name, "Unknown")
    assert(type(err.message) == "string")
end)

test("vector math and conversions", function()
    local a = nre.vec3(1, 2, 3)
    local b = nre.vec3({ 4, 5, 6 })
    local c = nre.vec3({ x = 1, y = 1, z = 1 })
    eq(a + b, nre.Vec3(5, 7, 9), "add")
    eq(b - a, nre.Vec3(3, 3, 3), "sub")
    eq(a * 2, nre.Vec3(2, 4, 6), "vec*num")
    eq(2 * a, nre.Vec3(2, 4, 6), "num*vec")
    eq(a * c, a, "componentwise")
    eq(-a, nre.Vec3(-1, -2, -3), "unm")
    eq(a:dot(b), 32, "dot")
    eq(nre.Vec3(1, 0, 0):cross(nre.Vec3(0, 1, 0)), nre.Vec3(0, 0, 1), "cross")
    eq(tostring(a), "Vec3(1, 2, 3)")
    assert(a ~= nil, "vec3 ~= nil")
    eq(nre.vec2(3, 4):length(), 5, "vec2 length")
    eq(nre.quat(), nre.Quat(0, 0, 0, 1), "identity quat")
    local t = nre.transform()
    eq(t.position, nre.Vec3(0, 0, 0)); eq(t.rotation, nre.Quat(0, 0, 0, 1)); eq(t.scale, nre.Vec3(1, 1, 1))
    t = nre.transform({ position = { 1, 2, 3 } })
    eq(t.position, a); eq(t.scale, nre.Vec3(1, 1, 1))
end)

test("nre.vertex accepts every vertex form", function()
    local want = nre.Vertex({ 1, 2, 3 }, { 0, 1, 0 }, { 0.5, 0.25 })
    local function same(v, what)
        eq(v.position, want.position, what .. " position"); eq(v.normal, want.normal, what .. " normal")
        eq(v.uv, want.uv, what .. " uv")
    end
    same(nre.vertex({ 1, 2, 3 }, { 0, 1, 0 }, { 0.5, 0.25 }), "args")
    same(nre.vertex({ position = { 1, 2, 3 }, normal = { x = 0, y = 1, z = 0 }, uv = { 0.5, 0.25 } }), "named")
    same(nre.vertex({ { 1, 2, 3 }, { 0, 1, 0 }, { 0.5, 0.25 } }), "positional")
    same(nre.vertex(want), "Vertex")
    local v = nre.vertex({ position = nre.Vec3(1, 2, 3) })
    eq(v.normal, nre.Vec3(0, 0, 0), "missing normal is zero"); eq(v.uv, nre.Vec2(0, 0), "missing uv is zero")
end)

-- The CDEF block and the wrappers against include/nre.h itself (skipped when the bindings are used outside the repo).
local header = io.open(here .. "/../../../include/nre.h")
if header then
    local h = header:read("*a")
    header:close()
    local src = assert(io.open(here .. "/../nre.lua")):read("*a")
    local cdef = assert(src:match("local CDEF = %[%[(.-)%]%]"), "no CDEF block in nre.lua")

    test("CDEF declares exactly nre.h's functions", function()
        local in_header, in_cdef = {}, {}
        for name in h:gmatch("NRE_C_EXPORT[^;]-%f[%w](nre%u%w*)%s*%(") do in_header[name] = true end
        for name in cdef:gmatch("%f[%w](nre%u%w*)%s*%(") do in_cdef[name] = true end
        local n = 0
        for name in pairs(in_header) do
            n = n + 1
            assert(in_cdef[name], name .. " is in nre.h but not in CDEF")
            assert(nre.lib[name], name .. " doesn't resolve in libnre_c")
        end
        for name in pairs(in_cdef) do assert(in_header[name], name .. " is in CDEF but not in nre.h") end
        eq(n, 89, "number of functions in nre.h")
    end)

    test("every nre.h function has a snake_case wrapper", function()
        for name in h:gmatch("NRE_C_EXPORT[^;]-%f[%w]nre(%u%w*)%s*%(") do
            local snake = name:gsub("(%l)(%u)", "%1_%2"):gsub("(%u)(%u%l)", "%1_%2"):lower()
            assert(type(nre[snake]) == "function", "no nre." .. snake .. " for nre" .. name)
        end
    end)

    test("CDEF enums and structs match nre.h", function()
        -- Lists of "NAME=value" enum constants and "Name{ fields }" structs, in order, comments stripped.
        local function constants(src)
            local out = {}
            for cname, value in src:gsub("//[^\n]*", ""):gmatch("(NRE_[%w_]+)%s*=%s*(%w+)") do
                out[#out + 1] = cname .. "=" .. value
            end
            return out
        end
        local function structs(src)
            local out = {}
            for body, name in src:gsub("//[^\n]*", ""):gmatch("typedef struct Nre%w+%s*(%b{})%s*(Nre%w+);") do
                out[#out + 1] = name .. body:gsub("%s+", " ")
            end
            return out
        end
        local function same_lists(got, want, what)
            for i = 1, math.max(#got, #want) do
                if got[i] ~= want[i] then
                    error(("%s #%d: nre.h has %s, CDEF has %s"):format(what, i, tostring(want[i]), tostring(got[i])), 2)
                end
            end
        end
        same_lists(constants(cdef), constants(h), "enum constant")
        -- nre.h declares the handles through NRE_DEFINE_HANDLE, which CDEF has expanded: compare those separately.
        local handles = {}
        for name in h:gmatch("NRE_DEFINE_HANDLE%((Nre%w+)%)") do
            handles[#handles + 1] = name .. "{ uint32_t index; uint32_t generation; }"
        end
        local cdef_structs, cdef_handles = {}, {}
        for _, s in ipairs(structs(cdef)) do
            local list = s:find("{ uint32_t index; uint32_t generation; }", 1, true) and cdef_handles or cdef_structs
            list[#list + 1] = s
        end
        same_lists(cdef_handles, handles, "handle")
        same_lists(cdef_structs, structs(h), "struct")
    end)
end

test("handles: empty handle, equality, tostring", function()
    local m = nre.Model()
    assert(m:is_empty())
    eq(m, nre.Model(), "empty == empty")
    assert(m ~= nil, "handle ~= nil")
    assert(not (m == nre.Texture()), "different handle types are not equal")
    eq(tostring(m), "Model(0, 0)")
end)

test("calls before set_render_config abort the process (child process)", function()
    -- The renderer has no context yet, so its error policy is Panic: it prints and aborts.
    local cmd = ("luajit -e 'package.path=%q..package.path; local nre=require(\"nre\"); "
        .. "pcall(nre.create_model, \"x.glb\"); print(\"SURVIVED\")' 2>&1"):format(here .. "/../?.lua;")
    local p = io.popen(cmd)
    local out = p:read("*a")
    p:close()
    assert(out:find("NotInitialized", 1, true), "expected a NotInitialized message, got: " .. out)
    assert(not out:find("SURVIVED", 1, true), "expected the child to abort, got: " .. out)
end)

-- From here on the renderer is configured (error_policy RETURN), but init() is never called: no window.
test("set_render_config (no window yet)", function()
    nre.set_render_config(320, 240, "headless", nil, nil, nre.RenderErrorPolicy.RETURN)
end)

test("second set_render_config raises ALREADY_INITIALIZED", function()
    local err = expect_error(nre.ErrorCode.ALREADY_INITIALIZED, nre.set_render_config, 1, 1, "again")
    eq(err.name, "AlreadyInitialized")
    assert(#err.message > 0, "empty message")
    eq(err.message, nre.get_last_error_message(), "message is the last error message")
    assert(tostring(err):find("AlreadyInitialized", 1, true), "__tostring")
end)

test("empty handle raises INVALID_HANDLE", function()
    local err = expect_error(nre.ErrorCode.INVALID_HANDLE, nre.get_model_materials, nre.Model())
    assert(#err.message > 0, "empty message")
    eq(err.message, nre.get_last_error_message(), "message is the last error message")
end)

test("missing model file raises FILE_NOT_FOUND", function()
    local err = expect_error(nre.ErrorCode.FILE_NOT_FOUND, nre.create_model, "/nonexistent/model.glb")
    assert(err.message:find("model.glb", 1, true), "message names the file: " .. err.message)
end)

test("nil handle means empty where the C API takes {0}", function()
    -- set_skybox(nil) reaches C (NoSceneBound, since no scene exists) instead of failing the FFI conversion.
    expect_error(nre.ErrorCode.NO_SCENE_BOUND, nre.set_skybox, nil)
end)

test("enum arguments accept numbers, short names and C names", function()
    eq(nre.is_key_down(nre.Key.W), false)
    eq(nre.is_key_down("W"), false)
    eq(nre.is_key_down("NRE_KEY_W"), false)
    eq(nre.is_mouse_button_down("LEFT"), false)
    assert(not pcall(nre.is_key_down, "NOT_A_KEY"), "bad enum name must raise")
end)

test("replace_model_material checks its 1-based index", function()
    expect_error(nre.ErrorCode.INDEX_OUT_OF_RANGE, nre.replace_model_material, nre.Model(), 0, nre.Material())
end)

test("create_mesh_model checks its arguments before calling C", function()
    local tri = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } }
    local err = expect_error(nre.ErrorCode.INVALID_ARGUMENT, nre.create_mesh_model, tri, { 0, 1, 2 })
    assert(err.message:find("1-based", 1, true), "message mentions 1-based: " .. err.message)
    expect_error(nre.ErrorCode.INVALID_ARGUMENT, nre.create_mesh_model, tri, { 1, 2.5, 3 })
    expect_error(nre.ErrorCode.INVALID_ARGUMENT, nre.create_mesh_model, "tri", { 1, 2, 3 })
    -- A pointer has no element count, so only arrays pass straight through.
    expect_error(nre.ErrorCode.INVALID_ARGUMENT, nre.create_mesh_model, ffi.cast("NreVertex*", nil), { 1, 2, 3 })
    expect_error(nre.ErrorCode.INVALID_ARGUMENT, nre.create_mesh_model, tri, ffi.new("int32_t[3]"))
end)

test("scene lighting calls need a bound scene", function()
    expect_error(nre.ErrorCode.NO_SCENE_BOUND, nre.set_scene_lighting, nre.SceneLighting.LANDSCAPE)
    expect_error(nre.ErrorCode.NO_SCENE_BOUND, nre.get_scene_lighting)
    expect_error(nre.ErrorCode.NO_SCENE_BOUND, nre.set_haze, 5000)
end)

print(("%d passed, %d failed"):format(passed, failures))
os.exit(failures == 0 and 0 or 1)

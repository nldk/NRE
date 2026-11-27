import CNre

// Math types with the layout of nre.h's. Angles are in radians; Mat4 is column-major (like glm and GLSL).

public struct Vec2: Hashable, Sendable {
    public var x: Float
    public var y: Float

    public init(_ x: Float, _ y: Float) { self.x = x; self.y = y }
    public init(_ v: SIMD2<Float>) { self.init(v.x, v.y) }

    public static let zero = Vec2(0, 0)
    public var simd: SIMD2<Float> { SIMD2(x, y) }

    init(_ c: NreVec2) { self.init(c.x, c.y) }
    var c: NreVec2 { NreVec2(x: x, y: y) }
}

public struct Vec3: Hashable, Sendable {
    public var x: Float
    public var y: Float
    public var z: Float

    public init(_ x: Float, _ y: Float, _ z: Float) { self.x = x; self.y = y; self.z = z }
    /// All three components set to `v`.
    public init(repeating v: Float) { self.init(v, v, v) }
    public init(_ v: SIMD3<Float>) { self.init(v.x, v.y, v.z) }

    public static let zero = Vec3(0, 0, 0)
    public static let one = Vec3(1, 1, 1)
    public var simd: SIMD3<Float> { SIMD3(x, y, z) }

    public static func + (a: Vec3, b: Vec3) -> Vec3 { Vec3(a.x + b.x, a.y + b.y, a.z + b.z) }
    public static func - (a: Vec3, b: Vec3) -> Vec3 { Vec3(a.x - b.x, a.y - b.y, a.z - b.z) }
    public static prefix func - (a: Vec3) -> Vec3 { Vec3(-a.x, -a.y, -a.z) }
    public static func * (a: Vec3, s: Float) -> Vec3 { Vec3(a.x * s, a.y * s, a.z * s) }
    public static func * (s: Float, a: Vec3) -> Vec3 { a * s }
    public static func / (a: Vec3, s: Float) -> Vec3 { Vec3(a.x / s, a.y / s, a.z / s) }
    public static func += (a: inout Vec3, b: Vec3) { a = a + b }
    public static func -= (a: inout Vec3, b: Vec3) { a = a - b }
    public static func *= (a: inout Vec3, s: Float) { a = a * s }

    init(_ c: NreVec3) { self.init(c.x, c.y, c.z) }
    var c: NreVec3 { NreVec3(x: x, y: y, z: z) }
}

/// A rotation quaternion, stored x, y, z, w (w is the real part).
public struct Quat: Hashable, Sendable {
    public var x: Float
    public var y: Float
    public var z: Float
    public var w: Float

    public init(_ x: Float, _ y: Float, _ z: Float, _ w: Float) { self.x = x; self.y = y; self.z = z; self.w = w }

    public static let identity = Quat(0, 0, 0, 1)

    init(_ c: NreQuat) { self.init(c.x, c.y, c.z, c.w) }
    var c: NreQuat { NreQuat(x: x, y: y, z: z, w: w) }
}

/// A 4x4 matrix: 16 floats, column-major (`elements[column * 4 + row]`).
public struct Mat4: Hashable, Sendable {
    public var elements: [Float]

    /// `elements` must hold 16 floats, column-major.
    public init(_ elements: [Float]) {
        precondition(elements.count == 16, "Mat4 needs 16 elements")
        self.elements = elements
    }

    public static let identity = Mat4([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])

    public subscript(column: Int, row: Int) -> Float {
        get { elements[column * 4 + row] }
        set { elements[column * 4 + row] = newValue }
    }

    init(_ c: NreMat4) {
        var m = c
        elements = withUnsafeBytes(of: &m.m) { Array($0.bindMemory(to: Float.self)) }
    }
}

/// Position, rotation and scale of an object in world space. Applied as scale, then rotation, then translation.
public struct Transform: Hashable, Sendable {
    public var position: Vec3
    public var rotation: Quat
    public var scale: Vec3

    public init(position: Vec3 = .zero, rotation: Quat = .identity, scale: Vec3 = .one) {
        self.position = position
        self.rotation = rotation
        self.scale = scale
    }

    public static let identity = Transform()

    init(_ c: NreTransform) { self.init(position: Vec3(c.position), rotation: Quat(c.rotation), scale: Vec3(c.scale)) }
    var c: NreTransform { NreTransform(position: position.c, rotation: rotation.c, scale: scale.c) }
}

/// One vertex for NRE.createMeshModel. A normal of `.zero` (the default) is computed from the triangles using
/// the vertex.
public struct Vertex: Hashable, Sendable {
    public var position: Vec3
    public var normal: Vec3
    public var uv: Vec2

    public init(position: Vec3, normal: Vec3 = .zero, uv: Vec2 = .zero) {
        self.position = position
        self.normal = normal
        self.uv = uv
    }

    /// Whether Vertex has NreVertex's layout (32 bytes: position at 0, normal at 12, uv at 24, floats packed),
    /// so a `[Vertex]` can go to C without a copy. Swift doesn't promise a struct's layout, so it's checked
    /// rather than assumed; it holds on every platform Swift supports today.
    static let hasCLayout: Bool =
        MemoryLayout<Vertex>.size == MemoryLayout<NreVertex>.size
            && MemoryLayout<Vertex>.stride == MemoryLayout<NreVertex>.stride
            && MemoryLayout<Vertex>.alignment == MemoryLayout<NreVertex>.alignment
            && MemoryLayout<Vertex>.offset(of: \.position) == MemoryLayout<NreVertex>.offset(of: \.position)
            && MemoryLayout<Vertex>.offset(of: \.normal) == MemoryLayout<NreVertex>.offset(of: \.normal)
            && MemoryLayout<Vertex>.offset(of: \.uv) == MemoryLayout<NreVertex>.offset(of: \.uv)
            && MemoryLayout<Vec3>.size == MemoryLayout<NreVec3>.size
            && MemoryLayout<Vec3>.offset(of: \.y) == 4 && MemoryLayout<Vec3>.offset(of: \.z) == 8
            && MemoryLayout<Vec2>.size == MemoryLayout<NreVec2>.size && MemoryLayout<Vec2>.offset(of: \.y) == 4

    init(_ c: NreVertex) { self.init(position: Vec3(c.position), normal: Vec3(c.normal), uv: Vec2(c.uv)) }
    var c: NreVertex { NreVertex(position: position.c, normal: normal.c, uv: uv.c) }
}

/// What NRE.pickInstance hit.
public struct RayHit: Hashable, Sendable {
    public var instance: Instance
    /// From the camera along the ray, in world units.
    public var distance: Float
    /// World space.
    public var point: Vec3

    public init(instance: Instance, distance: Float, point: Vec3) {
        self.instance = instance
        self.distance = distance
        self.point = point
    }

    init(_ c: NreRayHit) { self.init(instance: Instance(c.instance), distance: c.distance, point: Vec3(c.point)) }
}

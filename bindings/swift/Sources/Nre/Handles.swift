import CNre

// Handles: small values (index + generation) to copy around, like renderer.h's Handel<T>. Nothing destroys
// them for you: call the matching NRE.destroyXxx. `.empty` (all zeros) is the empty handle. Where the C API
// takes the empty handle ("no texture", "no skybox"), these bindings take an Optional and pass nil as
// `.empty`. (Not called `.none`: on an Optional parameter `.none` would be ambiguous with Optional.none.)

public struct Shader: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Shader(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreShader) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreShader { NreShader(index: index, generation: generation) }
}

public struct Pipeline: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Pipeline(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NrePipeline) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NrePipeline { NrePipeline(index: index, generation: generation) }
}

/// The loaded file (meshes, GPU buffers, materials); drawn through instances or NRE.drawModel.
public struct Model: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Model(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreModel) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreModel { NreModel(index: index, generation: generation) }
}

/// A copy of a model with its own transform.
public struct Instance: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Instance(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreInstance) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreInstance { NreInstance(index: index, generation: generation) }
}

public struct Light: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Light(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreLight) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreLight { NreLight(index: index, generation: generation) }
}

public struct Scene: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Scene(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreScene) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreScene { NreScene(index: index, generation: generation) }
}

public struct Texture: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Texture(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreTexture) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreTexture { NreTexture(index: index, generation: generation) }
}

public struct Material: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Material(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreMaterial) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreMaterial { NreMaterial(index: index, generation: generation) }
}

public struct Cubemap: Hashable, Sendable {
    let index: UInt32, generation: UInt32
    public static let empty = Cubemap(index: 0, generation: 0)
    public var isEmpty: Bool { self == .empty }
    init(_ c: NreCubemap) { index = c.index; generation = c.generation }
    init(index: UInt32, generation: UInt32) { self.index = index; self.generation = generation }
    var c: NreCubemap { NreCubemap(index: index, generation: generation) }
}

// Optional handles go to C as the empty handle when nil.
extension Optional where Wrapped == Texture { var c: NreTexture { (self ?? .empty).c } }
extension Optional where Wrapped == Cubemap { var c: NreCubemap { (self ?? .empty).c } }
extension Optional where Wrapped == Material { var c: NreMaterial { (self ?? .empty).c } }

import CNre

/// A failed NRE call: its error code and nreGetLastErrorMessage(), read right after the call.
public struct NreError: Error, Hashable, Sendable, CustomStringConvertible {
    public var code: ErrorCode
    public var message: String

    public init(code: ErrorCode, message: String) {
        self.code = code
        self.message = message
    }

    public var description: String { message.isEmpty ? "\(code)" : "\(code): \(message)" }
}

// The ABI these bindings were written against: nre.h 0.3.x.
let supportedMajor: UInt32 = 0
let supportedMinor: UInt32 = 3

/// nil if a libnre_c of this version is the ABI these bindings target (0.3.x), the error otherwise.
func versionError(major: UInt32, minor: UInt32, patch: UInt32) -> NreError? {
    if major == supportedMajor && minor == supportedMinor { return nil }
    return NreError(code: .libraryLoadFailed,
                    message: "libnre_c is version \(major).\(minor).\(patch), but these Swift bindings need "
                        + "\(supportedMajor).\(supportedMinor).x")
}

/// versionError for the loaded libnre_c. Swift initializes globals lazily and once, so this runs on the first
/// NRE call.
let versionMismatch: NreError? = {
    var major: UInt32 = 0, minor: UInt32 = 0, patch: UInt32 = 0
    nreGetVersion(&major, &minor, &patch)
    return versionError(major: major, minor: minor, patch: patch)
}()

@inline(__always)
func requireCompatibleLibrary() throws(NreError) {
    if let error = versionMismatch { throw error }
}

/// Calls an nre.h function that returns only an error code.
@inline(__always)
func call(_ body: () -> NreErrorCode) throws(NreError) {
    try requireCompatibleLibrary()
    try check(body())
}

/// Calls an nre.h function whose last parameter is an out-pointer, starting from `initial`, and returns what it wrote.
@inline(__always)
func get<T>(_ initial: T, _ body: (UnsafeMutablePointer<T>) -> NreErrorCode) throws(NreError) -> T {
    try requireCompatibleLibrary()
    var value = initial
    let code = withUnsafeMutablePointer(to: &value, body)
    try check(code)
    return value
}

@inline(__always)
func check(_ code: NreErrorCode) throws(NreError) {
    if code == NRE_OK { return }
    throw NreError(code: ErrorCode(code), message: String(cString: nreGetLastErrorMessage()))
}

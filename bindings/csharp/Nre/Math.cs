using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Nre;

// The math types of nre.h, plus its other plain structs (Vertex, RayHit). They have the C structs' layout and are
// passed to the C API as they are. Angles are in radians. They convert to and from System.Numerics for anything
// more than the basics.

/// <summary>NreVec2.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Vec2(float X, float Y)
{
    public static Vec2 Zero => default;

    public static Vec2 operator +(Vec2 a, Vec2 b) => new(a.X + b.X, a.Y + b.Y);
    public static Vec2 operator -(Vec2 a, Vec2 b) => new(a.X - b.X, a.Y - b.Y);
    public static Vec2 operator -(Vec2 v) => new(-v.X, -v.Y);
    public static Vec2 operator *(Vec2 v, float s) => new(v.X * s, v.Y * s);
    public static Vec2 operator *(float s, Vec2 v) => v * s;

    public static implicit operator Vec2((float X, float Y) t) => new(t.X, t.Y);
    public static implicit operator Vec2(Vector2 v) => new(v.X, v.Y);
    public static implicit operator Vector2(Vec2 v) => new(v.X, v.Y);
}

/// <summary>NreVec3.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Vec3(float X, float Y, float Z)
{
    public static Vec3 Zero => default;
    public static Vec3 One => new(1, 1, 1);

    public static Vec3 operator +(Vec3 a, Vec3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vec3 operator -(Vec3 a, Vec3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vec3 operator -(Vec3 v) => new(-v.X, -v.Y, -v.Z);
    public static Vec3 operator *(Vec3 v, float s) => new(v.X * s, v.Y * s, v.Z * s);
    public static Vec3 operator *(float s, Vec3 v) => v * s;

    public static implicit operator Vec3((float X, float Y, float Z) t) => new(t.X, t.Y, t.Z);
    public static implicit operator Vec3(Vector3 v) => new(v.X, v.Y, v.Z);
    public static implicit operator Vector3(Vec3 v) => new(v.X, v.Y, v.Z);
}

/// <summary>NreQuat: x, y, z, w (the same order as System.Numerics.Quaternion). Identity: (0, 0, 0, 1).</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Quat(float X, float Y, float Z, float W)
{
    public static Quat Identity => new(0, 0, 0, 1);

    /// <summary>A rotation of <paramref name="angle"/> radians around <paramref name="axis"/> (normalized here).</summary>
    public static Quat FromAxisAngle(Vec3 axis, float angle) => Quaternion.CreateFromAxisAngle(Vector3.Normalize(axis), angle);

    public static implicit operator Quat(Quaternion q) => new(q.X, q.Y, q.Z, q.W);
    public static implicit operator Quaternion(Quat q) => new(q.X, q.Y, q.Z, q.W);
}

/// <summary>
/// NreTransform: applied as scale, then rotation, then translation. Use <see cref="Identity"/> as the starting
/// point (<c>Transform.Identity with { Position = (0, 1, 0) }</c>); <c>default</c> has a zero rotation and scale.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Transform(Vec3 Position, Quat Rotation, Vec3 Scale)
{
    public static Transform Identity => new(Vec3.Zero, Quat.Identity, Vec3.One);

    public Transform(Vec3 position) : this(position, Quat.Identity, Vec3.One) { }
}

/// <summary>
/// NreVertex: one vertex for <see cref="NRE.CreateMeshModel"/>. A <see cref="Normal"/> of (0, 0, 0), the default,
/// is computed from the triangles that use the vertex.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Vertex(Vec3 Position, Vec3 Normal = default, Vec2 Uv = default);

/// <summary>
/// NreRayHit: what <see cref="NRE.PickInstance"/> hit. <see cref="Distance"/> is along the ray from the camera, in
/// world units; <see cref="Point"/> is in world space.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct RayHit(Instance Instance, float Distance, Vec3 Point);

/// <summary>NreMat4: 16 floats, column-major (like glm and GLSL): element <c>[column * 4 + row]</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct Mat4 : IEquatable<Mat4>
{
    [InlineArray(16)]
    private struct Floats16
    {
        private float _element0;
    }

    private readonly Floats16 _m;

    /// <summary>Builds a matrix from 16 column-major values.</summary>
    public Mat4(ReadOnlySpan<float> columnMajor)
    {
        if (columnMajor.Length != 16) throw new ArgumentException("a Mat4 needs 16 values", nameof(columnMajor));
        columnMajor.CopyTo(_m);
    }

    public static Mat4 Identity => new([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);

    /// <summary>Element <c>index</c> in memory order (column * 4 + row).</summary>
    public float this[int index] => ((ReadOnlySpan<float>)_m)[index];

    public float this[int column, int row] => this[column * 4 + row];

    public float[] ToArray() => ((ReadOnlySpan<float>)_m).ToArray();

    /// <summary>
    /// The same matrix as a System.Numerics.Matrix4x4. Its memory layout is the same as NRE's (translation in
    /// M41..M43), because System.Numerics uses row vectors where glm uses column vectors.
    /// </summary>
    public Matrix4x4 ToMatrix4x4()
    {
        ReadOnlySpan<float> m = _m;
        return new Matrix4x4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                             m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
    }

    public bool Equals(Mat4 other) => ((ReadOnlySpan<float>)_m).SequenceEqual(other._m);
    public override bool Equals(object? obj) => obj is Mat4 other && Equals(other);
    public override int GetHashCode()
    {
        var hash = new HashCode();
        foreach (float f in (ReadOnlySpan<float>)_m) hash.Add(f);
        return hash.ToHashCode();
    }
    public static bool operator ==(Mat4 a, Mat4 b) => a.Equals(b);
    public static bool operator !=(Mat4 a, Mat4 b) => !a.Equals(b);
    public override string ToString() => $"Mat4({string.Join(", ", ToArray())})";
}

using System.Runtime.InteropServices;

namespace Nre;

// The handles of nre.h: { uint32 index; uint32 generation; }, copied around by value. Their fields are the
// library's business, so they're internal. default (all zeros) is the empty handle, also available as None.
// Handles don't destroy anything by themselves: call the matching NRE.DestroyXxx, like the C++ API.

/// <summary>NreShader. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Shader
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Shader(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Shader None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Shader.None" : $"Shader({Index}, {Generation})";
}

/// <summary>NrePipeline. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Pipeline
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Pipeline(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Pipeline None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Pipeline.None" : $"Pipeline({Index}, {Generation})";
}

/// <summary>NreModel. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Model
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Model(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Model None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Model.None" : $"Model({Index}, {Generation})";
}

/// <summary>NreInstance. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Instance
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Instance(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Instance None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Instance.None" : $"Instance({Index}, {Generation})";
}

/// <summary>NreLight. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Light
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Light(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Light None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Light.None" : $"Light({Index}, {Generation})";
}

/// <summary>NreScene. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Scene
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Scene(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Scene None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Scene.None" : $"Scene({Index}, {Generation})";
}

/// <summary>NreTexture. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Texture
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Texture(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Texture None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Texture.None" : $"Texture({Index}, {Generation})";
}

/// <summary>NreMaterial. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Material
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Material(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Material None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Material.None" : $"Material({Index}, {Generation})";
}

/// <summary>NreCubemap. <see cref="None"/> (= <c>default</c>) is the empty handle.</summary>
[StructLayout(LayoutKind.Sequential)]
public readonly record struct Cubemap
{
    internal readonly uint Index;
    internal readonly uint Generation;

    internal Cubemap(uint index, uint generation) => (Index, Generation) = (index, generation);

    public static Cubemap None => default;
    public bool IsNone => Index == 0 && Generation == 0;
    public override string ToString() => IsNone ? "Cubemap.None" : $"Cubemap({Index}, {Generation})";
}

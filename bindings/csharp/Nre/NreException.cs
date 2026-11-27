namespace Nre;

/// <summary>
/// A failed NRE call: the C API's error code and the message from nreGetLastErrorMessage, read right after it.
/// </summary>
public sealed class NreException : Exception
{
    public ErrorCode Code { get; }

    /// <summary>The library's own message, without the code in front ("" if it gave none).</summary>
    public string NativeMessage { get; }

    public NreException(ErrorCode code, string nativeMessage)
        : base(string.IsNullOrEmpty(nativeMessage) ? code.ToString() : $"{code}: {nativeMessage}")
    {
        Code = code;
        NativeMessage = nativeMessage;
    }
}

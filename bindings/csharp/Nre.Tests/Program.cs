using Nre;

namespace Nre.Tests;

// The test assembly's entry point. The test runner doesn't use it; ChildProcessTests runs this assembly with
// `dotnet exec Nre.Tests.dll <probe>` for calls that may abort the process (the renderer panics on errors
// before setRenderConfig(), see HeadlessTests.CallsBeforeSetRenderConfigReportNotInitialized).
public static class Program
{
    public static int Main(string[] args)
    {
        Setup.DefaultLibDir();
        switch (args.FirstOrDefault())
        {
            case "create-model-before-config":
                try
                {
                    NRE.CreateModel("does-not-matter.glb");
                    Console.WriteLine("RETURNED");
                }
                catch (NreException e)
                {
                    Console.WriteLine($"CAUGHT {e.Code} {e.NativeMessage}");
                }
                return 0;
            case "get-version":
                try
                {
                    var (major, minor, patch) = NRE.GetVersion();
                    Console.WriteLine($"RETURNED {major}.{minor}.{patch}");
                }
                catch (NreException e)
                {
                    Console.WriteLine($"CAUGHT {e.Code} {e.NativeMessage}");
                }
                return 0;
            default:
                Console.Error.WriteLine("usage: Nre.Tests <probe>");
                return 2;
        }
    }
}

using System.IO;
using System.Reflection;
using Microsoft.CodeAnalysis.Testing;

namespace Godot.SourceGenerators.Tests;

public static class Constants
{
    public static Assembly GodotSharpAssembly => typeof(GodotObject).Assembly;

    // The testing library does not provide a predefined .NET 10 reference assembly set.
    public static ReferenceAssemblies Net100 => new ReferenceAssemblies(
        "net10.0",
        new PackageIdentity("Microsoft.NETCore.App.Ref", "10.0.0"),
        Path.Combine("ref", "net10.0")
    );

    public static string ExecutingAssemblyPath { get; }
    public static string SourceFolderPath { get; }
    public static string GeneratedSourceFolderPath { get; }

    static Constants()
    {
        ExecutingAssemblyPath = Path.GetFullPath(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location!)!);

        var testDataPath = Path.Combine(ExecutingAssemblyPath, "TestData");

        SourceFolderPath = Path.Combine(testDataPath, "Sources");
        GeneratedSourceFolderPath = Path.Combine(testDataPath, "GeneratedSources");
    }
}

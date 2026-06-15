// rpgkit — https://github.com/KirkDiggler/rpgkit

using UnrealBuildTool;
using System.IO;

public class RPGKitUE : ModuleRules
{
	public RPGKitUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		// rpgkit header-only library (vendored in ThirdParty/)
		string RPGKitPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "rpgkit", "core", "include");
		PublicIncludePaths.Add(RPGKitPath);

		// rpgkit requires C++20
		CppStandard = CppStandardVersion.Cpp20;
	}
}

// rpgkit — https://github.com/KirkDiggler/rpgkit

using UnrealBuildTool;
using System.Collections.Generic;

public class RPGKitUETarget : TargetRules
{
	public RPGKitUETarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("RPGKitUE");
	}
}

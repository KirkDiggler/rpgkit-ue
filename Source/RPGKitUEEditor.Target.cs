// rpgkit — https://github.com/KirkDiggler/rpgkit

using UnrealBuildTool;
using System.Collections.Generic;

public class RPGKitUEEditorTarget : TargetRules
{
	public RPGKitUEEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("RPGKitUE");
	}
}

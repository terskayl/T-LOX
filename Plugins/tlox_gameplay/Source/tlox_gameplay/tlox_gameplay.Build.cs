using UnrealBuildTool;
using System.IO;

public class tlox_gameplay : ModuleRules
{
	public tlox_gameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Projects" });

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
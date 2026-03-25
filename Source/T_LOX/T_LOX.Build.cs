// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;
public class T_LOX : ModuleRules
{
	public T_LOX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Projects" });

		PublicIncludePaths.Add(
			Path.Combine(ModuleDirectory, "..", "..", "tlox", "gameplay", "include")
		);

		PublicAdditionalLibraries.Add(
			Path.Combine(ModuleDirectory, "..", "..", "tlox", "build", "Release", "gameplay.lib")
		);

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

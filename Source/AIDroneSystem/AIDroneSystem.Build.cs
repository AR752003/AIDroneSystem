// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AIDroneSystem : ModuleRules
{
	public AIDroneSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "AITestSuite" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NetCommon", "UMG" });
	}
}

// Copyright GU5. All Rights Reserved.
using UnrealBuildTool;

public class UE5AIAUTO : ModuleRules
{
	public UE5AIAUTO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine",
			"Sockets", "Networking",
			"Json", "JsonUtilities",
			"EditorSubsystem", "UnrealEd",
			"ImageWrapper", "SlateCore",
			"AIModule", "GameplayTasks", "NavigationSystem",
			"UMG", "InputCore",
			"Niagara", "LevelSequence", "MovieScene", "PCG"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"BlueprintGraph", "KismetCompiler", "Kismet"
		});
	}
}

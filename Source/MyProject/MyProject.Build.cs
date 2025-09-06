// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyProject : ModuleRules
{
    public MyProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

    PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "NavigationSystem", "AIModule", "ProceduralMeshComponent", "GameplayTasks", "UMG", "Slate", "SlateCore", "NetCore", "Networking" });

        // Add public include paths for easier header inclusion (e.g., Character/CombatComponent.h)
        PublicIncludePaths.AddRange(new string[]
        {
            System.IO.Path.Combine(ModuleDirectory, "Character"),
            System.IO.Path.Combine(ModuleDirectory, "Enums"),
            System.IO.Path.Combine(ModuleDirectory, "Projectiles")
        });
    }
}

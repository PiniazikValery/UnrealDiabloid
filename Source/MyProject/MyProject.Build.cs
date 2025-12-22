// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyProject : ModuleRules
{
    public MyProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

    PublicDependencyModuleNames.AddRange(new string[] { 
        "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", 
        "NavigationSystem", "AIModule", "ProceduralMeshComponent", "GameplayTasks", 
        "UMG", "Slate", "SlateCore", "NetCore", "Networking",
        // Mass Entity System modules
        "MassEntity", "MassCommon", "MassMovement", "MassSpawner", 
        "MassRepresentation", "MassActors", "MassSignals", "MassNavigation",
        "MassSimulation"  // Required for processor execution!
    });

        // Add public include paths for easier header inclusion (e.g., Character/CombatComponent.h)
        // Add module root so includes can use "Folder/Header.h" (e.g., "Projectiles/MageProjectile.h").
        PublicIncludePaths.AddRange(new string[]
        {
            ModuleDirectory
        });
    }
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h" // ← ДОБАВЬТЕ

void UEnemyTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// ✅ ADD (not Require) built-in Mass fragments - we provide them ourselves
	BuildContext.AddFragment<FTransformFragment>();

	// Add all custom enemy fragments (Movement now contains Velocity)
	BuildContext.AddFragment<FEnemyTargetFragment>();
	BuildContext.AddFragment<FEnemyAttackFragment>();
	BuildContext.AddFragment<FEnemyMovementFragment>();
	BuildContext.AddFragment<FEnemyStateFragment>();
	BuildContext.AddFragment<FEnemyVisualizationFragment>();
	BuildContext.AddFragment<FEnemyNetworkFragment>();  // Phase 1: Network replication support

	// Add identifying tag
	BuildContext.AddTag<FEnemyTag>();

	// Note: In UE 5.6, fragments are added with default values from their constructors
	// Custom initialization happens in the spawner after entity creation
	// Processors are registered globally in their constructors with bAutoRegisterWithProcessingPhases = true
}

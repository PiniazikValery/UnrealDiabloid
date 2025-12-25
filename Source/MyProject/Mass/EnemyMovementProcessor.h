// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "EnemyFragments.h"
#include "Kismet/GameplayStatics.h"
#include "EnemyMovementProcessor.generated.h"

class UEnemySlotManagerSubsystem;

/**
 * Processor that handles enemy movement toward assigned slots around the player
 * Enemies are assigned slots in a formation around the player and navigate to their slot positions
 * 
 * Execution: Every frame in Movement phase (after avoidance)
 */
UCLASS()
class MYPROJECT_API UEnemyMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UEnemyMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	// Query that filters entities for processing
	FMassEntityQuery EntityQuery;

	// Cached player references (key = player index, avoid FindObject every frame)
	TMap<int32, TWeakObjectPtr<APawn>> CachedPlayerPawns;

	// Last time we refreshed the player list
	float LastPlayerRefreshTime = 0.0f;

	// How often to refresh the player list (in seconds)
	float PlayerRefreshInterval = 1.0f;

	// Cached slot manager subsystem
	TWeakObjectPtr<UEnemySlotManagerSubsystem> CachedSlotManager;

	// Frame counter for LOD system
	int32 FrameCounter = 0;
};

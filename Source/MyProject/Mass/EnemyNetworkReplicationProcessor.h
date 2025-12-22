// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h"
#include "EnemyFragments.h"
#include "MyProjectGameState.h"
#include "EnemyNetworkReplicationProcessor.generated.h"

/**
 * Server-side processor that replicates MASS entities to clients
 * Runs after movement processor to collect updated entity states
 * Batches updates and sends via GameState RPCs
 *
 * Execution: Server only, PrePhysics phase
 */
UCLASS()
class MYPROJECT_API UEnemyNetworkReplicationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UEnemyNetworkReplicationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/**
	 * Update network fragment with current entity state
	 */
	void UpdateNetworkFragment(
		const FTransformFragment& Transform,
		const FEnemyStateFragment& State,
		const FEnemyMovementFragment& Movement,
		const FEnemyAttackFragment& Attack,
		FEnemyNetworkFragment& Network,
		float DeltaTime);

	/**
	 * Compress entity state for network transmission
	 */
	FCompressedEnemyState CompressEntityState(
		const FTransformFragment& Transform,
		const FEnemyStateFragment& State,
		const FEnemyMovementFragment& Movement,
		const FEnemyAttackFragment& Attack,
		const FEnemyNetworkFragment& Network);

	/**
	 * Send batch updates to clients
	 */
	void SendBatchesToClients(const TMap<APlayerController*, TArray<FCompressedEnemyState>>& ClientBatches);

	// Query for entities that need replication
	FMassEntityQuery EntityQuery;

	// Maximum entities per batch (50 = ~1650 bytes per packet)
	int32 MaxEntitiesPerBatch = 50;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "EnemyFragments.h"
#include "MyProjectGameState.h"
#include "EnemyNetworkReceptionProcessor.generated.h"

/**
 * Client-side processor that receives MASS entity updates from server
 * Creates/updates local "shadow" entities based on server state
 *
 * Execution: Client only, PrePhysics phase
 */
UCLASS()
class MYPROJECT_API UEnemyNetworkReceptionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UEnemyNetworkReceptionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/**
	 * Process received batch updates from server
	 */
	void ProcessBatchUpdates(FMassEntityManager& EntityManager);

	/**
	 * Create a new client-side entity from server data
	 */
	FMassEntityHandle CreateClientEntity(
		FMassEntityManager& EntityManager,
		const FCompressedEnemyState& State);

	/**
	 * Update existing client-side entity from server data
	 */
	void UpdateClientEntity(
		FMassEntityManager& EntityManager,
		FMassEntityHandle EntityHandle,
		const FCompressedEnemyState& State);

	/**
	 * Decompress rotation from uint16 to degrees
	 */
	float DecompressRotationYaw(uint16 CompressedYaw) const;

	/**
	 * Decompress health from uint8 to 0-100 range
	 */
	float DecompressHealth(uint8 CompressedHealth) const;

	// Mapping: NetworkID -> Client EntityHandle
	TMap<int32, FMassEntityHandle> NetworkIDToEntity;
};

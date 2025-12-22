// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MyProjectGameState.h"
#include "MassEnemyReplicationSubsystem.generated.h"

/**
 * Server-side subsystem that manages MASS entity replication
 * Responsibilities:
 * - Tracks all players and their positions
 * - Assigns unique NetworkIDs to entities
 * - Calculates entity relevancy per client
 * - Manages replication priority and frequency
 */
UCLASS()
class MYPROJECT_API UMassEnemyReplicationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * Assign a unique NetworkID to an entity
	 * Called when entity is spawned
	 * @return The assigned NetworkID
	 */
	int32 AssignNetworkID();

	/**
	 * Release a NetworkID when entity is destroyed
	 * @param NetworkID The ID to release
	 */
	void ReleaseNetworkID(int32 NetworkID);

	/**
	 * Check if an entity is relevant to any client
	 * @param EntityLocation The entity's world location
	 * @param OutRelevantPlayers Output array of player controllers that can see this entity
	 * @return true if relevant to at least one client
	 */
	bool IsEntityRelevant(const FVector& EntityLocation, TArray<APlayerController*>& OutRelevantPlayers) const;

	/**
	 * Calculate replication priority based on distance
	 * @param EntityLocation The entity's world location
	 * @param PlayerLocation The player's camera location
	 * @return Priority value 0-255 (255 = highest priority)
	 */
	uint8 CalculateReplicationPriority(const FVector& EntityLocation, const FVector& PlayerLocation) const;

	/**
	 * Get replication interval based on distance
	 * @param Distance Distance from player to entity
	 * @return Minimum time between updates in seconds
	 */
	float GetReplicationInterval(float Distance) const;

	/**
	 * Get all active player controllers
	 * @return Array of player controllers
	 */
	TArray<APlayerController*> GetAllPlayerControllers() const;

	/**
	 * Get the relevancy radius (entities beyond this distance are not replicated)
	 */
	float GetRelevancyRadius() const { return RelevancyRadius; }

	/**
	 * Store batch data for client reception
	 * The reception processor will consume this data
	 */
	void StoreBatchForClient(APlayerController* Client, const FMassEntityBatchUpdate& Batch);

	/**
	 * Retrieve and clear stored batch data for a client
	 */
	bool GetAndClearBatchForClient(APlayerController* Client, FMassEntityBatchUpdate& OutBatch);

protected:
	// Next NetworkID to assign (incrementing counter)
	int32 NextNetworkID = 1;

	// Set of released NetworkIDs available for reuse
	TSet<int32> ReleasedNetworkIDs;

	// Relevancy radius in units (5000 = 50 meters)
	float RelevancyRadius = 5000.0f;

	// Update frequency thresholds (distance in units)
	float NearDistance = 1000.0f;   // 20 Hz updates
	float MidDistance = 2500.0f;    // 10 Hz updates
	float FarDistance = 5000.0f;    // 5 Hz updates

	// Update intervals in seconds
	float NearUpdateInterval = 0.05f;  // 20 Hz
	float MidUpdateInterval = 0.10f;   // 10 Hz
	float FarUpdateInterval = 0.20f;   // 5 Hz

	// Pending batch data for clients (consumed by reception processor)
	TMap<APlayerController*, FMassEntityBatchUpdate> PendingClientBatches;
};

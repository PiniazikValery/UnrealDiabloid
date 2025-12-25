// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "NavigationSystem.h"
#include "EnemySlotManagerSubsystem.generated.h"

/**
 * Represents a single slot around the player where an enemy can position
 */
USTRUCT()
struct FEnemySlot
{
	GENERATED_BODY()

	// Unique index of this slot
	int32 SlotIndex = INDEX_NONE;

	// Angle from player's forward (in degrees)
	float AngleFromPlayerForward = 0.0f;

	// Distance from player center
	float DistanceFromPlayer = 0.0f;

	// Current world position of this slot
	FVector WorldPosition = FVector::ZeroVector;

	// Entity handle occupying this slot (invalid if unoccupied)
	FMassEntityHandle OccupyingEntity;

	// Is this slot currently occupied?
	bool bIsOccupied = false;

	// Is this slot on a valid NavMesh location?
	bool bIsOnNavMesh = true;

	// Priority/desirability of this slot (lower = better, front slots preferred)
	float Priority = 0.0f;
};

/**
 * Per-player slot data container
 * Each player has their own set of slots around them
 */
USTRUCT()
struct FPlayerSlotData
{
	GENERATED_BODY()

	// All available slots around this player
	UPROPERTY()
	TArray<FEnemySlot> Slots;

	// Cached player location
	FVector CachedPlayerLocation = FVector::ZeroVector;

	// Cached player forward
	FVector CachedPlayerForward = FVector::ForwardVector;

	// Performance optimization: Cache slot validation results
	float LastSlotUpdateTime = 0.0f;
	float LastFullValidationTime = 0.0f;
};

/**
 * Manages slot allocation around players for enemy positioning
 * Creates a formation system where enemies occupy discrete positions around each player
 * Supports multiplayer - each player has their own set of slots
 */
UCLASS()
class MYPROJECT_API UEnemySlotManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * Update all slot world positions for a specific player
	 * Should be called each frame before enemy movement processing
	 * @param PlayerIndex Index of the player (0-3 typically)
	 * @param PlayerLocation Current world location of the player
	 * @param PlayerForward Current forward vector of the player
	 */
	void UpdateSlotPositions(int32 PlayerIndex, const FVector& PlayerLocation, const FVector& PlayerForward);

	/**
	 * Request a slot for an enemy entity around a specific player
	 * @param PlayerIndex Index of the player to get slots around
	 * @param EntityHandle The entity requesting a slot
	 * @param EntityLocation Current location of the requesting entity
	 * @param OutSlotPosition The world position of the assigned slot
	 * @return true if a slot was assigned, false if all slots are occupied
	 */
	bool RequestSlot(int32 PlayerIndex, FMassEntityHandle EntityHandle, const FVector& EntityLocation, FVector& OutSlotPosition);

	/**
	 * Release a slot occupied by an entity (searches all players)
	 * @param EntityHandle The entity releasing its slot
	 */
	void ReleaseSlot(FMassEntityHandle EntityHandle);

	/**
	 * Release slot by slot index for a specific player
	 * @param PlayerIndex Index of the player
	 * @param SlotIndex The index of the slot to release
	 */
	void ReleaseSlotByIndex(int32 PlayerIndex, int32 SlotIndex);

	/**
	 * Get the world position of a specific slot for a player
	 * @param PlayerIndex Index of the player
	 * @param SlotIndex Index of the slot
	 * @return World position of the slot, or ZeroVector if invalid
	 */
	FVector GetSlotWorldPosition(int32 PlayerIndex, int32 SlotIndex) const;

	/**
	 * Check if an entity has an assigned slot (searches all players)
	 * @param EntityHandle The entity to check
	 * @param OutPlayerIndex Output player index if found
	 * @param OutSlotIndex Output slot index if found
	 * @return true if entity has a slot assigned
	 */
	bool GetEntitySlot(FMassEntityHandle EntityHandle, int32& OutPlayerIndex, int32& OutSlotIndex) const;

	/**
	 * Get the current player location (cached)
	 * @param PlayerIndex Index of the player
	 */
	FVector GetCachedPlayerLocation(int32 PlayerIndex) const;

	/**
	 * Get number of available (unoccupied) slots for a player
	 * @param PlayerIndex Index of the player
	 */
	int32 GetAvailableSlotCount(int32 PlayerIndex) const;

	/**
	 * Check if a slot is on valid NavMesh
	 * @param PlayerIndex Index of the player
	 * @param SlotIndex Index of the slot
	 * @return true if slot is on NavMesh, false otherwise
	 */
	bool IsSlotOnNavMesh(int32 PlayerIndex, int32 SlotIndex) const;

	/**
	 * Get total number of slots per player
	 */
	int32 GetTotalSlotCount() const { return MaxSlots; }

	/**
	 * Get number of active players being tracked
	 */
	int32 GetActivePlayerCount() const { return PlayerSlotData.Num(); }

	/**
	 * Debug: Draw slot positions for a specific player
	 * @param PlayerIndex Index of the player (-1 for all players)
	 * @param Duration How long to display
	 */
	void DebugDrawSlots(int32 PlayerIndex = -1, float Duration = 0.0f) const;

protected:
	/**
	 * Generate the slot configuration (rings of slots around player)
	 */
	void GenerateSlots();

	/**
	 * Find the best available slot for an entity based on its position
	 */
	int32 FindBestAvailableSlot(const FVector& EntityLocation) const;

	/**
	 * Check if a position has enough clearance from nav mesh edges
	 * Tests points around the location to ensure enemies can navigate there
	 * @param NavSys The navigation system
	 * @param Location The position to check
	 * @param RequiredClearance Minimum distance from nav mesh edge required
	 * @return true if position has adequate clearance
	 */
	bool HasNavMeshClearance(UNavigationSystemV1* NavSys, const FVector& Location, float RequiredClearance) const;

	/**
	 * Try to find a safe slot position with adequate nav mesh clearance
	 * When original slot is too close to nav mesh edge, this finds an adjusted position
	 * @param NavSys The navigation system
	 * @param PlayerLocation Current player position
	 * @param SlotDirection Direction from player to slot
	 * @param OriginalDistance Original slot distance from player
	 * @param RequiredClearance Minimum nav mesh clearance needed
	 * @param OutPosition Output adjusted position with good clearance
	 * @return true if a safe position was found
	 */
	bool FindSafeSlotPosition(UNavigationSystemV1* NavSys, const FVector& PlayerLocation, const FVector& SlotDirection, float OriginalDistance, float RequiredClearance, FVector& OutPosition) const;

	/**
	 * Find the best available slot for an entity based on its position (internal helper)
	 * @param Slots The slot array to search
	 * @param EntityLocation Current location of the requesting entity
	 */
	int32 FindBestAvailableSlotInArray(const TArray<FEnemySlot>& Slots, const FVector& EntityLocation) const;

private:
	// Per-player slot data (key = player index)
	UPROPERTY()
	TMap<int32, FPlayerSlotData> PlayerSlotData;

	// Performance optimization intervals
	float SlotUpdateInterval = 0.5f; // Update every 0.5 seconds instead of every frame
	float FullValidationInterval = 2.0f; // Do expensive clearance checks every 2 seconds

	// Configuration

	// Maximum number of slots to generate per player
	int32 MaxSlots = 500;

	// Distance of first ring from player
	float FirstRingDistance = 100.0f;

	// Distance between rings
	float RingSpacing = 60.0f;

	// Number of slots in the first ring (increases for outer rings)
	int32 FirstRingSlotsCount = 8;

	// How many additional slots to add per ring
	int32 SlotsIncreasePerRing = 4;

	// Minimum attack range - innermost ring distance
	float MinSlotDistance = 80.0f;
};

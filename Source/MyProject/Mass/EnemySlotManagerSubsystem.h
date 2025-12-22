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
 * Manages slot allocation around the player for enemy positioning
 * Creates a formation system where enemies occupy discrete positions
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
	 * Update all slot world positions based on current player location
	 * Should be called each frame before enemy movement processing
	 */
	void UpdateSlotPositions(const FVector& PlayerLocation, const FVector& PlayerForward);

	/**
	 * Request a slot for an enemy entity
	 * @param EntityHandle The entity requesting a slot
	 * @param EntityLocation Current location of the requesting entity
	 * @param OutSlotPosition The world position of the assigned slot
	 * @return true if a slot was assigned, false if all slots are occupied
	 */
	bool RequestSlot(FMassEntityHandle EntityHandle, const FVector& EntityLocation, FVector& OutSlotPosition);

	/**
	 * Release a slot occupied by an entity
	 * @param EntityHandle The entity releasing its slot
	 */
	void ReleaseSlot(FMassEntityHandle EntityHandle);

	/**
	 * Release slot by slot index
	 * @param SlotIndex The index of the slot to release
	 */
	void ReleaseSlotByIndex(int32 SlotIndex);

	/**
	 * Get the world position of a specific slot
	 * @param SlotIndex Index of the slot
	 * @return World position of the slot, or ZeroVector if invalid
	 */
	FVector GetSlotWorldPosition(int32 SlotIndex) const;

	/**
	 * Check if an entity has an assigned slot
	 * @param EntityHandle The entity to check
	 * @param OutSlotIndex Output slot index if found
	 * @return true if entity has a slot assigned
	 */
	bool GetEntitySlot(FMassEntityHandle EntityHandle, int32& OutSlotIndex) const;

	/**
	 * Get the current player location (cached)
	 */
	FVector GetCachedPlayerLocation() const { return CachedPlayerLocation; }

	/**
	 * Get number of available (unoccupied) slots
	 */
	int32 GetAvailableSlotCount() const;

	/**
	 * Check if a slot is on valid NavMesh
	 * @param SlotIndex Index of the slot
	 * @return true if slot is on NavMesh, false otherwise
	 */
	bool IsSlotOnNavMesh(int32 SlotIndex) const;

	/**
	 * Get total number of slots
	 */
	int32 GetTotalSlotCount() const { return Slots.Num(); }

	/**
	 * Debug: Draw slot positions
	 */
	void DebugDrawSlots(float Duration = 0.0f) const;

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

private:
	// All available slots around the player
	UPROPERTY()
	TArray<FEnemySlot> Slots;

	// Cached player location
	FVector CachedPlayerLocation = FVector::ZeroVector;

	// Cached player forward
	FVector CachedPlayerForward = FVector::ForwardVector;

	// Performance optimization: Cache slot validation results
	float LastSlotUpdateTime = 0.0f;
	float SlotUpdateInterval = 0.5f; // Update every 0.5 seconds instead of every frame
	float LastFullValidationTime = 0.0f;
	float FullValidationInterval = 2.0f; // Do expensive clearance checks every 2 seconds

	// Configuration
	
	// Maximum number of slots to generate
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

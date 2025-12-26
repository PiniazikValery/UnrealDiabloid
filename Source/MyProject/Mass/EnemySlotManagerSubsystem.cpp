// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySlotManagerSubsystem.h"
#include "DrawDebugHelpers.h"

void UEnemySlotManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Slots are now generated per-player when UpdateSlotPositions is first called
	UE_LOG(LogTemp, Log, TEXT("EnemySlotManagerSubsystem: Initialized (per-player slots will be generated on demand)"));
}

void UEnemySlotManagerSubsystem::Deinitialize()
{
	PlayerSlotData.Empty();
	Super::Deinitialize();
}

bool UEnemySlotManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create this subsystem for all game worlds
	return true;
}

void UEnemySlotManagerSubsystem::GenerateSlots()
{
	// This function now generates slots into a provided array
	// Called internally when a new player is added
}

// Helper to generate slots into an array
static void GenerateSlotsIntoArray(TArray<FEnemySlot>& Slots, int32 MaxSlots, float FirstRingDistance, float RingSpacing, int32 FirstRingSlotsCount, int32 SlotsIncreasePerRing)
{
	Slots.Empty();
	Slots.Reserve(MaxSlots);

	int32 SlotIndex = 0;
	int32 RingIdx = 0;

	// Generate rings dynamically until we reach MaxSlots
	while (SlotIndex < MaxSlots)
	{
		// Calculate ring distance: starts at FirstRingDistance, increases by RingSpacing each ring
		const float RingDistance = FirstRingDistance + (RingIdx * RingSpacing);

		// Calculate number of slots in this ring: starts at FirstRingSlotsCount, increases each ring
		const int32 NumSlotsInRing = FirstRingSlotsCount + (RingIdx * SlotsIncreasePerRing);

		const float AngleStep = 360.0f / NumSlotsInRing;

		// Priority increases with ring distance (inner rings are preferred)
		const float BasePriority = static_cast<float>(RingIdx);

		for (int32 SlotInRing = 0; SlotInRing < NumSlotsInRing && SlotIndex < MaxSlots; ++SlotInRing)
		{
			FEnemySlot NewSlot;
			NewSlot.SlotIndex = SlotIndex;
			NewSlot.AngleFromPlayerForward = SlotInRing * AngleStep;
			NewSlot.DistanceFromPlayer = RingDistance;
			NewSlot.bIsOccupied = false;

			// Priority: prefer front slots (angle close to 0 or 360) and inner rings
			// Normalize angle to 0-180 range for priority calculation
			float NormalizedAngle = NewSlot.AngleFromPlayerForward;
			if (NormalizedAngle > 180.0f)
			{
				NormalizedAngle = 360.0f - NormalizedAngle;
			}
			NewSlot.Priority = BasePriority + (NormalizedAngle / 180.0f);

			Slots.Add(NewSlot);
			SlotIndex++;
		}

		RingIdx++;
	}

	UE_LOG(LogTemp, Log, TEXT("EnemySlotManagerSubsystem: Generated %d slots across %d rings for player"),
		Slots.Num(), RingIdx);
}

void UEnemySlotManagerSubsystem::UpdateSlotPositions(int32 PlayerIndex, const FVector& PlayerLocation, const FVector& PlayerForward)
{
	// Get or create slot data for this player
	FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (!SlotData)
	{
		// First time seeing this player - create slots for them
		FPlayerSlotData NewSlotData;
		GenerateSlotsIntoArray(NewSlotData.Slots, MaxSlots, FirstRingDistance, RingSpacing, FirstRingSlotsCount, SlotsIncreasePerRing);
		PlayerSlotData.Add(PlayerIndex, MoveTemp(NewSlotData));
		SlotData = PlayerSlotData.Find(PlayerIndex);
		UE_LOG(LogTemp, Log, TEXT("EnemySlotManagerSubsystem: Created slot data for player %d"), PlayerIndex);
	}

	TArray<FEnemySlot>& Slots = SlotData->Slots;

	// PERFORMANCE FIX: Only update slots every SlotUpdateInterval seconds
	// This prevents 24,000+ NavMesh queries per frame near obstacles
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float TimeSinceLastUpdate = CurrentTime - SlotData->LastSlotUpdateTime;

	if (TimeSinceLastUpdate < SlotUpdateInterval)
	{
		// Too soon - skip expensive validation but still update positions with lightweight NavMesh projection
		SlotData->CachedPlayerLocation = PlayerLocation;
		SlotData->CachedPlayerForward = PlayerForward;
		SlotData->CachedPlayerForward.Z = 0.0f;
		if (!SlotData->CachedPlayerForward.IsNearlyZero())
		{
			SlotData->CachedPlayerForward.Normalize();
		}
		else
		{
			SlotData->CachedPlayerForward = FVector::ForwardVector;
		}

		// Quick position update with basic NavMesh projection (no clearance checks)
		// Project ALL slots to NavMesh for correct Z, but skip expensive clearance validation
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		const FVector NavSearchExtent(200.0f, 200.0f, 300.0f);

		for (FEnemySlot& Slot : Slots)
		{
			FVector SlotDirection = SlotData->CachedPlayerForward.RotateAngleAxis(Slot.AngleFromPlayerForward, FVector::UpVector);
			FVector DesiredPosition = PlayerLocation + SlotDirection * Slot.DistanceFromPlayer;

			if (NavSys)
			{
				FNavLocation NavLoc;
				if (NavSys->ProjectPointToNavigation(DesiredPosition, NavLoc, NavSearchExtent))
				{
					Slot.WorldPosition = NavLoc.Location;
					Slot.bIsOnNavMesh = true;
				}
				else
				{
					// Projection failed - keep last valid position
					// Update XY but preserve last valid Z
					FVector NewPos = DesiredPosition;
					NewPos.Z = Slot.WorldPosition.Z;
					Slot.WorldPosition = NewPos;
					Slot.bIsOnNavMesh = false;
				}
			}
			else
			{
				Slot.WorldPosition = DesiredPosition;
				Slot.bIsOnNavMesh = true;
			}
		}
		return;
	}

	// Time to do a real update with NavMesh validation
	SlotData->LastSlotUpdateTime = CurrentTime;

	SlotData->CachedPlayerLocation = PlayerLocation;
	SlotData->CachedPlayerForward = PlayerForward;
	SlotData->CachedPlayerForward.Z = 0.0f;
	if (!SlotData->CachedPlayerForward.IsNearlyZero())
	{
		SlotData->CachedPlayerForward.Normalize();
	}
	else
	{
		SlotData->CachedPlayerForward = FVector::ForwardVector;
	}

	// Get NavSystem for validation
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	const FVector NavSearchExtent(200.0f, 200.0f, 300.0f);  // Use larger extent to avoid fallback queries

	// Minimum clearance from nav mesh edge that an enemy needs to comfortably navigate
	// Reduced from 150 to 100 for better slot availability
	const float MinNavMeshClearance = 100.0f;

	// Determine if we should do expensive clearance checks this frame
	const bool bDoFullValidation = (CurrentTime - SlotData->LastFullValidationTime) >= FullValidationInterval;
	if (bDoFullValidation)
	{
		SlotData->LastFullValidationTime = CurrentTime;
	}

	// Update world position for each slot
	// OPTIMIZATION: Only validate slots that are occupied or near the player (within 400 units)
	for (FEnemySlot& Slot : Slots)
	{
		// Rotate from player forward by the slot's angle
		FVector SlotDirection = SlotData->CachedPlayerForward.RotateAngleAxis(Slot.AngleFromPlayerForward, FVector::UpVector);
		FVector DesiredPosition = PlayerLocation + SlotDirection * Slot.DistanceFromPlayer;

		// Skip expensive validation for distant unoccupied slots
		const bool bIsCloseToPlayer = Slot.DistanceFromPlayer <= 400.0f;
		const bool bNeedsValidation = Slot.bIsOccupied || bIsCloseToPlayer;

		// Validate against NavMesh (only for relevant slots)
		if (NavSys && bNeedsValidation)
		{
			FNavLocation NavLoc;
			// Use single larger search extent to avoid fallback queries
			if (NavSys->ProjectPointToNavigation(DesiredPosition, NavLoc, NavSearchExtent))
			{
				// Only do expensive clearance checks during full validation intervals
				bool bHasClearance = true;
				if (bDoFullValidation && bIsCloseToPlayer)
				{
					bHasClearance = HasNavMeshClearance(NavSys, NavLoc.Location, MinNavMeshClearance);
				}

				if (bHasClearance)
				{
					// Slot is on NavMesh with good clearance
					Slot.WorldPosition = NavLoc.Location;
					Slot.bIsOnNavMesh = true;
				}
				else
				{
					// Slot is too close to nav mesh edge - try to find better position
					FVector AdjustedPosition;
					if (FindSafeSlotPosition(NavSys, PlayerLocation, SlotDirection, Slot.DistanceFromPlayer, MinNavMeshClearance, AdjustedPosition))
					{
						Slot.WorldPosition = AdjustedPosition;
						Slot.bIsOnNavMesh = true;
					}
					else
					{
						// Couldn't find safe position - mark as invalid
						Slot.WorldPosition = DesiredPosition;
						Slot.bIsOnNavMesh = false;
					}
				}
			}
			else
			{
				// Slot is NOT on NavMesh (inside building, off map, etc.)
				Slot.WorldPosition = DesiredPosition;
				Slot.bIsOnNavMesh = false;
			}
		}
		else
		{
			Slot.WorldPosition = DesiredPosition;
			if (!NavSys)
			{
				Slot.bIsOnNavMesh = true; // Assume valid if no NavSys
			}
			// Otherwise keep previous bIsOnNavMesh state for distant slots
		}
	}
}

bool UEnemySlotManagerSubsystem::RequestSlot(int32 PlayerIndex, FMassEntityHandle EntityHandle, const FVector& EntityLocation, FVector& OutSlotPosition)
{
	// Check if entity already has a slot with any player
	int32 ExistingPlayerIndex = INDEX_NONE;
	int32 ExistingSlotIndex = INDEX_NONE;
	if (GetEntitySlot(EntityHandle, ExistingPlayerIndex, ExistingSlotIndex))
	{
		// Entity already has a slot
		if (ExistingPlayerIndex == PlayerIndex)
		{
			// Same player - check if a better (closer to player) slot is available
			FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
			if (SlotData && ExistingSlotIndex >= 0 && ExistingSlotIndex < SlotData->Slots.Num())
			{
				const FEnemySlot& CurrentSlot = SlotData->Slots[ExistingSlotIndex];

				// Find the best available slot (excluding our current one)
				int32 BestSlotIndex = INDEX_NONE;
				float BestSlotDistanceFromPlayer = FLT_MAX;

				for (int32 i = 0; i < SlotData->Slots.Num(); ++i)
				{
					const FEnemySlot& Slot = SlotData->Slots[i];

					// Skip occupied slots (but our current slot is occupied by us, so skip it too)
					if (Slot.bIsOccupied || !Slot.bIsOnNavMesh)
					{
						continue;
					}

					// Check if this slot is closer to the player than our current slot
					if (Slot.DistanceFromPlayer < BestSlotDistanceFromPlayer)
					{
						BestSlotDistanceFromPlayer = Slot.DistanceFromPlayer;
						BestSlotIndex = i;
					}
				}

				// If we found a slot that's significantly closer to the player, switch to it
				// Use a threshold to avoid constant slot swapping (at least one ring closer)
				const float MinImprovementThreshold = RingSpacing * 0.8f; // ~80% of ring spacing
				if (BestSlotIndex != INDEX_NONE &&
					(CurrentSlot.DistanceFromPlayer - BestSlotDistanceFromPlayer) >= MinImprovementThreshold)
				{
					// Release current slot
					SlotData->Slots[ExistingSlotIndex].bIsOccupied = false;
					SlotData->Slots[ExistingSlotIndex].OccupyingEntity = FMassEntityHandle();

					// Assign the better slot
					SlotData->Slots[BestSlotIndex].bIsOccupied = true;
					SlotData->Slots[BestSlotIndex].OccupyingEntity = EntityHandle;
					OutSlotPosition = SlotData->Slots[BestSlotIndex].WorldPosition;
					return true;
				}

				// No better slot available, keep current
				OutSlotPosition = CurrentSlot.WorldPosition;
				return true;
			}
		}
		else
		{
			// Different player - release old slot first
			ReleaseSlot(EntityHandle);
		}
	}

	// Get slot data for this player
	const FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (!SlotData)
	{
		// Player doesn't have slot data yet
		OutSlotPosition = FVector::ZeroVector;
		return false;
	}

	// Find best available slot
	int32 BestSlotIndex = FindBestAvailableSlotInArray(SlotData->Slots, EntityLocation);

	if (BestSlotIndex == INDEX_NONE)
	{
		// No slots available - entity should move toward player anyway
		OutSlotPosition = SlotData->CachedPlayerLocation;
		return false;
	}

	// Assign the slot (need non-const access)
	FPlayerSlotData* MutableSlotData = PlayerSlotData.Find(PlayerIndex);
	MutableSlotData->Slots[BestSlotIndex].bIsOccupied = true;
	MutableSlotData->Slots[BestSlotIndex].OccupyingEntity = EntityHandle;
	OutSlotPosition = MutableSlotData->Slots[BestSlotIndex].WorldPosition;

	return true;
}

void UEnemySlotManagerSubsystem::ReleaseSlot(FMassEntityHandle EntityHandle)
{
	// Search all players for this entity's slot
	for (auto& Pair : PlayerSlotData)
	{
		for (FEnemySlot& Slot : Pair.Value.Slots)
		{
			if (Slot.bIsOccupied && Slot.OccupyingEntity == EntityHandle)
			{
				Slot.bIsOccupied = false;
				Slot.OccupyingEntity = FMassEntityHandle();
				return;
			}
		}
	}
}

void UEnemySlotManagerSubsystem::ReleaseSlotByIndex(int32 PlayerIndex, int32 SlotIndex)
{
	FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (SlotData && SlotIndex >= 0 && SlotIndex < SlotData->Slots.Num())
	{
		SlotData->Slots[SlotIndex].bIsOccupied = false;
		SlotData->Slots[SlotIndex].OccupyingEntity = FMassEntityHandle();
	}
}

FVector UEnemySlotManagerSubsystem::GetSlotWorldPosition(int32 PlayerIndex, int32 SlotIndex) const
{
	const FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (SlotData && SlotIndex >= 0 && SlotIndex < SlotData->Slots.Num())
	{
		return SlotData->Slots[SlotIndex].WorldPosition;
	}
	return FVector::ZeroVector;
}

bool UEnemySlotManagerSubsystem::GetEntitySlot(FMassEntityHandle EntityHandle, int32& OutPlayerIndex, int32& OutSlotIndex) const
{
	// Search all players for this entity's slot
	for (const auto& Pair : PlayerSlotData)
	{
		const TArray<FEnemySlot>& Slots = Pair.Value.Slots;
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].bIsOccupied && Slots[i].OccupyingEntity == EntityHandle)
			{
				OutPlayerIndex = Pair.Key;
				OutSlotIndex = i;
				return true;
			}
		}
	}
	OutPlayerIndex = INDEX_NONE;
	OutSlotIndex = INDEX_NONE;
	return false;
}

FVector UEnemySlotManagerSubsystem::GetCachedPlayerLocation(int32 PlayerIndex) const
{
	const FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (SlotData)
	{
		return SlotData->CachedPlayerLocation;
	}
	return FVector::ZeroVector;
}

int32 UEnemySlotManagerSubsystem::GetAvailableSlotCount(int32 PlayerIndex) const
{
	const FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (!SlotData)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FEnemySlot& Slot : SlotData->Slots)
	{
		if (!Slot.bIsOccupied)
		{
			Count++;
		}
	}
	return Count;
}

bool UEnemySlotManagerSubsystem::IsSlotOnNavMesh(int32 PlayerIndex, int32 SlotIndex) const
{
	const FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
	if (SlotData && SlotIndex >= 0 && SlotIndex < SlotData->Slots.Num())
	{
		return SlotData->Slots[SlotIndex].bIsOnNavMesh;
	}
	return false;
}

int32 UEnemySlotManagerSubsystem::FindBestAvailableSlotInArray(const TArray<FEnemySlot>& Slots, const FVector& EntityLocation) const
{
	int32 BestSlotIndex = INDEX_NONE;
	float BestScore = FLT_MAX;
	
	// First pass: Check if enemy is already very close to a high-priority (inner ring) slot
	// If so, claim that slot immediately to prevent running past good positions
	const float NearbySlotThreshold = 150.0f;  // If within this distance, consider the slot "nearby"
	int32 NearbyHighPrioritySlotIndex = INDEX_NONE;
	float NearbyHighPrioritySlotDistance = FLT_MAX;
	float NearbyHighPrioritySlotRingDist = FLT_MAX;
	
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FEnemySlot& Slot = Slots[i];
		
		if (Slot.bIsOccupied || !Slot.bIsOnNavMesh)
		{
			continue;
		}
		
		float DistanceToEnemy = FVector::Dist(EntityLocation, Slot.WorldPosition);
		
		// Check if this slot is nearby
		if (DistanceToEnemy <= NearbySlotThreshold)
		{
			// Among nearby slots, prefer the one with highest priority (closest to player)
			// If same ring distance, prefer the one closest to the enemy
			if (Slot.DistanceFromPlayer < NearbyHighPrioritySlotRingDist ||
				(Slot.DistanceFromPlayer == NearbyHighPrioritySlotRingDist && DistanceToEnemy < NearbyHighPrioritySlotDistance))
			{
				NearbyHighPrioritySlotIndex = i;
				NearbyHighPrioritySlotDistance = DistanceToEnemy;
				NearbyHighPrioritySlotRingDist = Slot.DistanceFromPlayer;
			}
		}
	}
	
	// If we found a nearby high-priority slot, use it
	if (NearbyHighPrioritySlotIndex != INDEX_NONE)
	{
		return NearbyHighPrioritySlotIndex;
	}
	
	// Second pass: Standard scoring - find the best slot overall
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FEnemySlot& Slot = Slots[i];
		
		if (Slot.bIsOccupied)
		{
			continue;
		}
		
		// Skip slots that are not on NavMesh (inside buildings, off map, etc.)
		if (!Slot.bIsOnNavMesh)
		{
			continue;
		}
		
		// Score based on slot distance from player (inner rings = higher priority)
		// Primary priority: slots closest to the player character (inner rings first)
		// Secondary: among slots at same distance, prefer ones closer to the enemy for faster arrival
		float DistanceToEnemy = FVector::Dist(EntityLocation, Slot.WorldPosition);
		
		// Use the slot's configured distance from player as the primary score
		// This ensures inner rings (closer to player) are always preferred
		const float RingPriorityWeight = 1000.0f;  // High weight to ensure inner rings win
		const float EnemyDistanceWeight = 1.0f;    // Low weight - only breaks ties within same ring
		
		// Lower score = better slot
		// Slot.DistanceFromPlayer is the ring distance (180, 280, 380, etc.)
		float Score = (Slot.DistanceFromPlayer * RingPriorityWeight) + (DistanceToEnemy * EnemyDistanceWeight);
		
		if (Score < BestScore)
		{
			BestScore = Score;
			BestSlotIndex = i;
		}
	}
	
	return BestSlotIndex;
}

bool UEnemySlotManagerSubsystem::HasNavMeshClearance(UNavigationSystemV1* NavSys, const FVector& Location, float RequiredClearance) const
{
	if (!NavSys)
	{
		return true;
	}
	
	// PERFORMANCE FIX: Reduced from 16 total queries (8 outer + 8 inner) to 4 queries
	// Test points in 4 cardinal directions only - sufficient for most cases
	const int32 NumDirections = 4;
	const FVector TestExtent(50.0f, 50.0f, 150.0f);  // Larger extent for better NavMesh snapping
	int32 ValidDirections = 0;
	
	// Test at required clearance distance (only outer ring, skip inner ring check)
	for (int32 i = 0; i < NumDirections; ++i)
	{
		float Angle = (360.0f / NumDirections) * i;
		FVector TestDir = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector);
		FVector TestPoint = Location + TestDir * RequiredClearance;
		
		FNavLocation NavLoc;
		if (NavSys->ProjectPointToNavigation(TestPoint, NavLoc, TestExtent))
		{
			// More lenient tolerance (50 units instead of 30) for better slot availability
			float ProjectionDist = FVector::Dist2D(TestPoint, NavLoc.Location);
			if (ProjectionDist < 50.0f)
			{
				ValidDirections++;
			}
		}
	}
	
	// Require at least 3 out of 4 directions (75% coverage)
	// Less strict than before but still ensures reasonable clearance
	return ValidDirections >= 3;
}

bool UEnemySlotManagerSubsystem::FindSafeSlotPosition(UNavigationSystemV1* NavSys, const FVector& PlayerLocation, const FVector& SlotDirection, float OriginalDistance, float RequiredClearance, FVector& OutPosition) const
{
	if (!NavSys)
	{
		return false;
	}
	
	const FVector SearchExtent(50.0f, 50.0f, 200.0f);
	
	// Try pulling the slot closer to the player in steps
	// Start from 80% of original distance and work down to 50%
	for (float DistanceMultiplier = 0.8f; DistanceMultiplier >= 0.5f; DistanceMultiplier -= 0.1f)
	{
		float TestDistance = OriginalDistance * DistanceMultiplier;
		// Don't go closer than minimum attack range
		if (TestDistance < 80.0f)
		{
			TestDistance = 80.0f;
		}
		
		FVector TestPosition = PlayerLocation + SlotDirection * TestDistance;
		FNavLocation NavLoc;
		
		if (NavSys->ProjectPointToNavigation(TestPosition, NavLoc, SearchExtent))
		{
			if (HasNavMeshClearance(NavSys, NavLoc.Location, RequiredClearance))
			{
				OutPosition = NavLoc.Location;
				return true;
			}
		}
	}
	
	// If pulling closer didn't work, try slight angle adjustments
	const float AngleAdjustments[] = { 15.0f, -15.0f, 30.0f, -30.0f, 45.0f, -45.0f };
	for (float AngleOffset : AngleAdjustments)
	{
		FVector AdjustedDir = SlotDirection.RotateAngleAxis(AngleOffset, FVector::UpVector);
		
		// Try at various distances
		for (float DistanceMultiplier = 1.0f; DistanceMultiplier >= 0.6f; DistanceMultiplier -= 0.2f)
		{
			float TestDistance = OriginalDistance * DistanceMultiplier;
			if (TestDistance < 80.0f)
			{
				TestDistance = 80.0f;
			}
			
			FVector TestPosition = PlayerLocation + AdjustedDir * TestDistance;
			FNavLocation NavLoc;
			
			if (NavSys->ProjectPointToNavigation(TestPosition, NavLoc, SearchExtent))
			{
				if (HasNavMeshClearance(NavSys, NavLoc.Location, RequiredClearance))
				{
					OutPosition = NavLoc.Location;
					return true;
				}
			}
		}
	}
	
	return false;
}

void UEnemySlotManagerSubsystem::DebugDrawSlots(int32 PlayerIndex, float Duration) const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// IMPORTANT: Use persistent debug drawing to avoid crash with ComponentsThatNeedEndOfFrameUpdate_OnGameThread
	// The crash occurs because non-persistent debug draws create temporary components that get tracked
	// in an internal array, and when called from Mass processors during frame execution, the array
	// indices can become invalid by the time end-of-frame cleanup runs.
	// Using bPersistent=true avoids this issue entirely.
	const bool bPersistent = true;
	const float ActualDuration = (Duration < 0.0f) ? 0.1f : Duration;

	// Flush previous persistent debug lines before drawing new ones
	FlushPersistentDebugLines(World);

	// Colors for different players
	static const FColor PlayerColors[] = { FColor::Cyan, FColor::Magenta, FColor::Yellow, FColor::Orange };
	const int32 NumPlayerColors = UE_ARRAY_COUNT(PlayerColors);

	// Draw slots for specified player or all players
	auto DrawPlayerSlots = [&](int32 PIdx, const FPlayerSlotData& SlotData)
	{
		FColor PlayerColor = PlayerColors[PIdx % NumPlayerColors];

		// Draw ring circles around player - calculate ring distances dynamically
		// Only draw first few rings to avoid clutter
		const int32 MaxRingsToDraw = 10;
		for (int32 RingIdx = 0; RingIdx < MaxRingsToDraw; ++RingIdx)
		{
			float RingDist = FirstRingDistance + (RingIdx * RingSpacing);
			DrawDebugCircle(World, SlotData.CachedPlayerLocation, RingDist, 32, PlayerColor, bPersistent, ActualDuration, 0, 1.0f, FVector::RightVector, FVector::ForwardVector, false);
		}

		// Draw all slots
		for (const FEnemySlot& Slot : SlotData.Slots)
		{
			// Skip drawing invalid slots that are off NavMesh
			if (!Slot.bIsOnNavMesh)
			{
				// Draw invalid slots as small gray X
				DrawDebugPoint(World, Slot.WorldPosition, 10.0f, FColor(80, 80, 80), bPersistent, ActualDuration);
				continue;
			}

			if (Slot.bIsOccupied)
			{
				// Occupied slot - red sphere
				DrawDebugSphere(World, Slot.WorldPosition, 30.0f, 8, FColor::Red, bPersistent, ActualDuration);
			}
			else
			{
				// Available slot - green sphere
				DrawDebugSphere(World, Slot.WorldPosition, 25.0f, 6, FColor::Green, bPersistent, ActualDuration);
			}

			// Draw line from slot to player center
			DrawDebugLine(World, Slot.WorldPosition, SlotData.CachedPlayerLocation, PlayerColor, bPersistent, ActualDuration, 0, 0.5f);
		}

		// Draw player center with forward direction
		DrawDebugSphere(World, SlotData.CachedPlayerLocation, 40.0f, 8, FColor::White, bPersistent, ActualDuration);
		DrawDebugDirectionalArrow(World, SlotData.CachedPlayerLocation, SlotData.CachedPlayerLocation + SlotData.CachedPlayerForward * 150.0f, 50.0f, PlayerColor, bPersistent, ActualDuration, 0, 3.0f);
	};

	if (PlayerIndex < 0)
	{
		// Draw all players
		for (const auto& Pair : PlayerSlotData)
		{
			DrawPlayerSlots(Pair.Key, Pair.Value);
		}
	}
	else
	{
		// Draw specific player
		const FPlayerSlotData* SlotData = PlayerSlotData.Find(PlayerIndex);
		if (SlotData)
		{
			DrawPlayerSlots(PlayerIndex, *SlotData);
		}
	}
#endif
}

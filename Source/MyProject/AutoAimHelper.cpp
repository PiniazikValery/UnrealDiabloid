// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAimHelper.h"
#include "EnemyCharacter.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "MassEntitySubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"
#include "Mass/EnemyFragments.h"
#include "Mass/MassEnemyReplicationSubsystem.h"
#include "Mass/MassEnemySpawner.h"
#include "Mass/EnemyVisualizationProcessor.h"
#include "Kismet/GameplayStatics.h"

UAutoAimHelper::UAutoAimHelper()
{
}

FAutoAimResult UAutoAimHelper::FindBestTargetAndAngle(
	AActor* SourceActor,
	TSubclassOf<AActor> TargetClass,
	float SearchRange,
	float MaxAngleDegrees,
	ETargetSelectionMode SelectionMode)
{
	FAutoAimResult Result;
	
	if (!SourceActor || !SourceActor->GetWorld())
	{
		return Result;
	}
	
	// Default to AEnemyCharacter if no class specified
	if (!TargetClass)
	{
		TargetClass = AEnemyCharacter::StaticClass();
	}
	
	// Find all potential targets
	TArray<AActor*> PotentialTargets = FindTargetsInArc(
		SourceActor,
		TargetClass,
		SearchRange,
		MaxAngleDegrees
	);
	
	if (PotentialTargets.Num() == 0)
	{
		return Result;
	}
	
	// Find best target based on selection mode
	AActor* BestTarget = nullptr;
	float BestScore = FLT_MAX;
	float BestAngle = 0.f;
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	
	for (AActor* Target : PotentialTargets)
	{
		float AngleToTarget = 0.f;
		const FVector TargetLocation = Target->GetActorLocation();
		
		// Calculate angle BEFORE arc check so we can see it
		AngleToTarget = CalculateAimAngleToTarget(SourceActor, Target);
		
		// Verify target is in arc (should be, but double-check)
		float ArcCheckAngle = 0.f;
		if (!IsTargetInFrontArc(SourceActor, TargetLocation, MaxAngleDegrees, ArcCheckAngle))
		{
			continue;
		}
		
		// Calculate score
		float Score = CalculateTargetScore(SourceActor, Target, AngleToTarget, SelectionMode);
		
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Target;
			BestAngle = AngleToTarget;
		}
	}
	
	// Fill result
	if (BestTarget)
	{
		Result.Target = BestTarget;
		Result.AimAngle = BestAngle;
		Result.DistanceToTarget = FVector::Dist(SourceLocation, BestTarget->GetActorLocation());
		Result.bTargetFound = true;
	}
	
	return Result;
}

float UAutoAimHelper::CalculateAimAngleToTarget(AActor* SourceActor, AActor* Target)
{
	if (!SourceActor || !Target)
	{
		return 0.f;
	}
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector SourceForward = SourceActor->GetActorForwardVector();
	const FRotator SourceRotation = SourceActor->GetActorRotation();
	const FVector TargetLocation = Target->GetActorLocation();
	
	// Calculate direction to target (2D only, ignore Z)
	FVector DirectionToTarget = TargetLocation - SourceLocation;
	DirectionToTarget.Z = 0.f;
	DirectionToTarget.Normalize();
	
	// Calculate raw angles
	float TargetAtan2 = FMath::Atan2(DirectionToTarget.Y, DirectionToTarget.X);
	float ForwardAtan2 = FMath::Atan2(SourceForward.Y, SourceForward.X);
	float RawAngleDegrees = FMath::RadiansToDegrees(TargetAtan2 - ForwardAtan2);
	
	// Normalize to -180 to 180 range
	float AngleDegrees = RawAngleDegrees;
	while (AngleDegrees > 180.f) AngleDegrees -= 360.f;
	while (AngleDegrees < -180.f) AngleDegrees += 360.f;
	
	// Mirror the angle by negating it
	float MirroredAngle = -AngleDegrees;
	
	// Return mirrored angle
	return MirroredAngle;
}

bool UAutoAimHelper::IsTargetInFrontArc(
	AActor* SourceActor,
	FVector TargetLocation,
	float MaxAngleDegrees,
	float& OutAngle)
{
	if (!SourceActor)
	{
		return false;
	}
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector SourceForward = SourceActor->GetActorForwardVector();
	
	// Calculate direction to target (2D)
	FVector DirectionToTarget = TargetLocation - SourceLocation;
	DirectionToTarget.Z = 0.f;
	
	if (DirectionToTarget.IsNearlyZero())
	{
		return false;
	}
	
	DirectionToTarget.Normalize();
	
	// Calculate signed angle using atan2
	float AngleDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(DirectionToTarget.Y, DirectionToTarget.X) - 
		FMath::Atan2(SourceForward.Y, SourceForward.X)
	);
	
	// Normalize to -180 to 180 range
	while (AngleDegrees > 180.f) AngleDegrees -= 360.f;
	while (AngleDegrees < -180.f) AngleDegrees += 360.f;
	
	// Check if within arc (absolute value)
	if (FMath::Abs(AngleDegrees) <= MaxAngleDegrees)
	{
		OutAngle = AngleDegrees;
		return true;
	}
	
	return false;
}

TArray<AActor*> UAutoAimHelper::FindTargetsInArc(
	AActor* SourceActor,
	TSubclassOf<AActor> TargetClass,
	float SearchRange,
	float MaxAngleDegrees)
{
	TArray<AActor*> ValidTargets;
	
	if (!SourceActor || !SourceActor->GetWorld())
	{
		return ValidTargets;
	}
	
	const FVector SourceLocation = SourceActor->GetActorLocation();
	
	// Setup sphere overlap
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRange);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(SourceActor);
	
	// Find all overlapping actors
	TArray<FOverlapResult> OverlapResults;
	SourceActor->GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		SourceLocation,
		FQuat::Identity,
		ECC_Pawn,
		SphereShape,
		QueryParams
	);
	
	// Filter by class and arc
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		AActor* Actor = Overlap.GetActor();
		
		// Check class
		if (!Actor || !Actor->IsA(TargetClass))
		{
			continue;
		}
		
		// Check if in front arc
		float AngleToTarget = 0.f;
		if (IsTargetInFrontArc(SourceActor, Actor->GetActorLocation(), MaxAngleDegrees, AngleToTarget))
		{
			ValidTargets.Add(Actor);
		}
	}
	
	return ValidTargets;
}

float UAutoAimHelper::CalculateTargetScore(
	AActor* SourceActor,
	AActor* Target,
	float AngleToTarget,
	ETargetSelectionMode SelectionMode)
{
	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist(SourceLocation, TargetLocation);
	
	switch (SelectionMode)
	{
		case ETargetSelectionMode::ClosestToCenter:
			// Lower angle = better (closer to center)
			return FMath::Abs(AngleToTarget);
			
		case ETargetSelectionMode::ClosestByDistance:
			// Lower distance = better
			return Distance;
			
		case ETargetSelectionMode::LowestHealth:
		{
			// Try to get health from target
			// You'll need to implement IHealthInterface or similar
			// For now, fallback to distance
			return Distance;
		}
			
		case ETargetSelectionMode::HighestThreat:
		{
			// Combine factors: closer + more centered = higher threat
			// Normalize both factors and combine
			const float NormalizedAngle = FMath::Abs(AngleToTarget) / 90.f; // 0-1
			const float NormalizedDistance = Distance / 1000.f; // 0-1 (assuming max 1000 range)
			return (NormalizedAngle + NormalizedDistance) * 0.5f;
		}
			
		default:
			return Distance;
	}
}

float UAutoAimHelper::CalculateAimAngleToLocation(AActor* SourceActor, FVector TargetLocation)
{
	if (!SourceActor)
	{
		return 0.f;
	}

	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector SourceForward = SourceActor->GetActorForwardVector();

	// Calculate direction to target (2D only, ignore Z)
	FVector DirectionToTarget = TargetLocation - SourceLocation;
	DirectionToTarget.Z = 0.f;
	DirectionToTarget.Normalize();

	// Calculate raw angles
	float TargetAtan2 = FMath::Atan2(DirectionToTarget.Y, DirectionToTarget.X);
	float ForwardAtan2 = FMath::Atan2(SourceForward.Y, SourceForward.X);
	float RawAngleDegrees = FMath::RadiansToDegrees(TargetAtan2 - ForwardAtan2);

	// Normalize to -180 to 180 range
	float AngleDegrees = RawAngleDegrees;
	while (AngleDegrees > 180.f) AngleDegrees -= 360.f;
	while (AngleDegrees < -180.f) AngleDegrees += 360.f;

	// Mirror the angle by negating it (matching existing behavior)
	return -AngleDegrees;
}

FMassAutoAimResult UAutoAimHelper::FindBestMassEntityTarget(
	AActor* SourceActor,
	float SearchRange,
	float MaxAngleDegrees,
	ETargetSelectionMode SelectionMode,
	bool bCheckVisibility)
{
	FMassAutoAimResult Result;

	UE_LOG(LogTemp, Warning, TEXT("[AutoAim] FindBestMassEntityTarget called - SearchRange: %.1f, MaxAngle: %.1f"), SearchRange, MaxAngleDegrees);

	if (!SourceActor || !SourceActor->GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AutoAim] FAILED: SourceActor or World is null"));
		return Result;
	}

	UWorld* World = SourceActor->GetWorld();

	// Get Mass Entity Subsystem
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AutoAim] FAILED: EntitySubsystem is null"));
		return Result;
	}

	// Get the replication subsystem which tracks all entities
	UMassEnemyReplicationSubsystem* ReplicationSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>();
	if (!ReplicationSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AutoAim] FAILED: ReplicationSubsystem is null"));
		return Result;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector SourceForward = SourceActor->GetActorForwardVector();
	const float SearchRangeSquared = SearchRange * SearchRange;

	// Check if we're on server or client
	const bool bIsServer = World->GetNetMode() != NM_Client;
	UE_LOG(LogTemp, Warning, TEXT("[AutoAim] SourceLocation: %s, SourceForward: %s, IsServer: %s"),
		*SourceLocation.ToString(), *SourceForward.ToString(), bIsServer ? TEXT("true") : TEXT("false"));

	// Best target tracking
	float BestScore = FLT_MAX;
	FVector BestLocation = FVector::ZeroVector;
	float BestAngle = 0.f;
	float BestDistance = 0.f;
	FMassEntityHandle BestEntity;
	int32 BestNetworkID = INDEX_NONE;

	// Get entities to iterate - different source for server vs client
	// Client: uses NetworkIDToEntity map (populated from replication)
	// Server: uses spawner's tracked entities array
	TMap<int32, FMassEntityHandle>& ClientEntityMap = ReplicationSubsystem->GetNetworkIDToEntityMap();

	// Build a combined list of entities to check
	TArray<TPair<int32, FMassEntityHandle>> EntitiesToCheck;

	if (!bIsServer && ClientEntityMap.Num() > 0)
	{
		// Client with replicated entities
		for (auto& Pair : ClientEntityMap)
		{
			EntitiesToCheck.Add(TPair<int32, FMassEntityHandle>(Pair.Key, Pair.Value));
		}
		UE_LOG(LogTemp, Warning, TEXT("[AutoAim] Client mode - using %d entities from replication map"), EntitiesToCheck.Num());
	}
	else
	{
		// Server mode - get entities from the spawner's tracked array
		// Find the MassEnemySpawner in the world
		TArray<AActor*> FoundSpawners;
		UGameplayStatics::GetAllActorsOfClass(World, AMassEnemySpawner::StaticClass(), FoundSpawners);

		if (FoundSpawners.Num() > 0)
		{
			AMassEnemySpawner* Spawner = Cast<AMassEnemySpawner>(FoundSpawners[0]);
			if (Spawner)
			{
				const TArray<FMassEntityHandle>& SpawnedEntities = Spawner->GetSpawnedEntities();

				for (const FMassEntityHandle& Handle : SpawnedEntities)
				{
					if (EntityManager.IsEntityValid(Handle))
					{
						// Get network ID if available
						const FEnemyNetworkFragment* Network = EntityManager.GetFragmentDataPtr<FEnemyNetworkFragment>(Handle);
						int32 NetworkID = Network ? Network->NetworkID : INDEX_NONE;
						EntitiesToCheck.Add(TPair<int32, FMassEntityHandle>(NetworkID, Handle));
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("[AutoAim] Server mode - using %d entities from spawner (total tracked: %d)"),
					EntitiesToCheck.Num(), SpawnedEntities.Num());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[AutoAim] Server mode - spawner cast failed"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AutoAim] Server mode - no MassEnemySpawner found in world"));
		}
	}

	int32 ValidCount = 0;
	int32 AliveCount = 0;
	int32 InRangeCount = 0;
	int32 InArcCount = 0;
	int32 VisibleCount = 0;

	for (auto& Pair : EntitiesToCheck)
	{
		const int32 NetworkID = Pair.Key;
		const FMassEntityHandle& EntityHandle = Pair.Value;

		// Verify entity is valid
		if (!EntityManager.IsEntityValid(EntityHandle))
		{
			continue;
		}
		ValidCount++;

		// Get fragments - entities in the replication map are created with a consistent archetype
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
		const FEnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FEnemyStateFragment>(EntityHandle);

		// Skip if fragments are missing (shouldn't happen but be safe)
		if (!Transform || !State)
		{
			continue;
		}

		// Skip dead enemies
		if (!State->bIsAlive)
		{
			continue;
		}
		AliveCount++;

		const FVector EntityLocation = Transform->GetTransform().GetLocation();

		// Distance check (squared for performance)
		const float DistanceSquared = FVector::DistSquared(SourceLocation, EntityLocation);
		if (DistanceSquared > SearchRangeSquared)
		{
			continue;
		}
		InRangeCount++;

		const float Distance = FMath::Sqrt(DistanceSquared);

		// Calculate direction to target (2D)
		FVector DirectionToTarget = EntityLocation - SourceLocation;
		DirectionToTarget.Z = 0.f;

		if (DirectionToTarget.IsNearlyZero())
		{
			continue;
		}

		DirectionToTarget.Normalize();

		// Calculate angle
		float AngleDegrees = FMath::RadiansToDegrees(
			FMath::Atan2(DirectionToTarget.Y, DirectionToTarget.X) -
			FMath::Atan2(SourceForward.Y, SourceForward.X)
		);

		// Normalize to -180 to 180 range
		while (AngleDegrees > 180.f) AngleDegrees -= 360.f;
		while (AngleDegrees < -180.f) AngleDegrees += 360.f;

		// Check if within arc
		if (FMath::Abs(AngleDegrees) > MaxAngleDegrees)
		{
			continue;
		}
		InArcCount++;

		// Visibility check via line trace
		if (bCheckVisibility)
		{
			FHitResult HitResult;
			FCollisionQueryParams TraceParams;
			TraceParams.AddIgnoredActor(SourceActor);
			TraceParams.bTraceComplex = false;

			// Trace from source to entity (aiming at center mass, slightly elevated)
			FVector TraceEnd = EntityLocation + FVector(0.f, 0.f, 50.f); // Aim at chest height
			FVector TraceStart = SourceLocation + FVector(0.f, 0.f, 50.f);

			bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				TraceStart,
				TraceEnd,
				ECC_Visibility,
				TraceParams
			);

			// If we hit something before reaching the target, it's blocked
			if (bHit && HitResult.Distance < Distance - 50.f)
			{
				continue; // Target is not visible
			}
		}
		VisibleCount++;

		// Calculate score based on selection mode
		float Score = 0.f;
		switch (SelectionMode)
		{
			case ETargetSelectionMode::ClosestToCenter:
				Score = FMath::Abs(AngleDegrees);
				break;
			case ETargetSelectionMode::ClosestByDistance:
				Score = Distance;
				break;
			case ETargetSelectionMode::HighestThreat:
			{
				const float NormalizedAngle = FMath::Abs(AngleDegrees) / MaxAngleDegrees;
				const float NormalizedDistance = Distance / SearchRange;
				Score = (NormalizedAngle + NormalizedDistance) * 0.5f;
				break;
			}
			default:
				Score = Distance;
				break;
		}

		if (Score < BestScore)
		{
			BestScore = Score;
			BestLocation = EntityLocation;
			BestAngle = -AngleDegrees; // Mirror angle to match existing behavior
			BestDistance = Distance;
			BestEntity = EntityHandle;
			BestNetworkID = NetworkID;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[AutoAim] Filter stats - Valid: %d, Alive: %d, InRange: %d, InArc: %d, Visible: %d"),
		ValidCount, AliveCount, InRangeCount, InArcCount, VisibleCount);

	// Fill result if we found a target
	if (BestScore < FLT_MAX)
	{
		Result.EntityHandle = BestEntity;
		Result.TargetLocation = BestLocation;
		Result.AimAngle = BestAngle;
		Result.DistanceToTarget = BestDistance;
		Result.bTargetFound = true;
		Result.TargetNetworkID = BestNetworkID;

		UE_LOG(LogTemp, Warning, TEXT("[AutoAim] SUCCESS - Found target NetworkID: %d, Angle: %.1f, Distance: %.1f, Location: %s"),
			BestNetworkID, BestAngle, BestDistance, *BestLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AutoAim] NO TARGET FOUND"));
	}

	return Result;
}

bool UAutoAimHelper::ApplyDamageToMassEntity(AActor* WorldContext, int32 TargetNetworkID, float Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] ApplyDamageToMassEntity called - NetworkID: %d, Damage: %.1f"), TargetNetworkID, Damage);

	if (!WorldContext || !WorldContext->GetWorld() || TargetNetworkID == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: Invalid WorldContext or NetworkID"));
		return false;
	}

	UWorld* World = WorldContext->GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: No EntitySubsystem"));
		return false;
	}

	UMassEnemyReplicationSubsystem* ReplicationSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>();
	if (!ReplicationSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: No ReplicationSubsystem"));
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Find entity by NetworkID
	TMap<int32, FMassEntityHandle>& NetworkIDMap = ReplicationSubsystem->GetNetworkIDToEntityMap();
	FMassEntityHandle* EntityHandlePtr = NetworkIDMap.Find(TargetNetworkID);

	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] ReplicationMap has %d entries, Found in map: %s"),
		NetworkIDMap.Num(), EntityHandlePtr ? TEXT("YES") : TEXT("NO"));

	if (!EntityHandlePtr || !EntityManager.IsEntityValid(*EntityHandlePtr))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Not in replication map, searching spawner..."));

		// Try to find in spawner's tracked entities (server-side)
		TArray<AActor*> FoundSpawners;
		UGameplayStatics::GetAllActorsOfClass(World, AMassEnemySpawner::StaticClass(), FoundSpawners);

		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Found %d spawners"), FoundSpawners.Num());

		if (FoundSpawners.Num() > 0)
		{
			AMassEnemySpawner* Spawner = Cast<AMassEnemySpawner>(FoundSpawners[0]);
			if (Spawner)
			{
				const TArray<FMassEntityHandle>& SpawnedEntities = Spawner->GetSpawnedEntities();
				UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Spawner has %d entities"), SpawnedEntities.Num());

				for (const FMassEntityHandle& Handle : SpawnedEntities)
				{
					if (EntityManager.IsEntityValid(Handle))
					{
						const FEnemyNetworkFragment* Network = EntityManager.GetFragmentDataPtr<FEnemyNetworkFragment>(Handle);
						if (Network && Network->NetworkID == TargetNetworkID)
						{
							UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Found entity with matching NetworkID %d in spawner"), TargetNetworkID);

							// Found the entity, now apply damage
							FEnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FEnemyStateFragment>(Handle);
							if (State && State->bIsAlive)
							{
								State->Health -= Damage;
								UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Applied %.1f damage to entity NetworkID %d, Health: %.1f/%.1f"),
									Damage, TargetNetworkID, State->Health, State->MaxHealth);

								if (State->Health <= 0.0f)
								{
									State->Health = 0.0f;
									State->bIsAlive = false;
									UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Entity NetworkID %d killed!"), TargetNetworkID);

									// Queue for destruction
									DestroyMassEntity(WorldContext, TargetNetworkID);
								}
								return true;
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Entity found but State invalid or dead. State: %s, Alive: %s"),
									State ? TEXT("valid") : TEXT("NULL"), State ? (State->bIsAlive ? TEXT("yes") : TEXT("no")) : TEXT("N/A"));
							}
							return false;
						}
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("[MassDamage] NetworkID %d not found in spawner entities"), TargetNetworkID);
			}
		}
		return false;
	}

	// Found entity in replication map
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Found entity in replication map"));
	FMassEntityHandle EntityHandle = *EntityHandlePtr;
	FEnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FEnemyStateFragment>(EntityHandle);

	if (!State || !State->bIsAlive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Entity state invalid or dead"));
		return false;
	}

	State->Health -= Damage;
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Applied %.1f damage to entity NetworkID %d, Health: %.1f/%.1f"),
		Damage, TargetNetworkID, State->Health, State->MaxHealth);

	if (State->Health <= 0.0f)
	{
		State->Health = 0.0f;
		State->bIsAlive = false;
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Entity NetworkID %d killed!"), TargetNetworkID);

		// Queue for destruction
		DestroyMassEntity(WorldContext, TargetNetworkID);
	}

	return true;
}

int32 UAutoAimHelper::ApplyDamageAtLocation(AActor* WorldContext, FVector HitLocation, float DamageRadius, float Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] ApplyDamageAtLocation called - Location: %s, Radius: %.1f, Damage: %.1f"),
		*HitLocation.ToString(), DamageRadius, Damage);

	if (!WorldContext || !WorldContext->GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: Invalid WorldContext"));
		return 0;
	}

	UWorld* World = WorldContext->GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: No EntitySubsystem"));
		return 0;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const float DamageRadiusSquared = DamageRadius * DamageRadius;
	int32 DamagedCount = 0;
	TArray<int32> EntitiesToDestroy;

	// Get spawner for entity iteration
	TArray<AActor*> FoundSpawners;
	UGameplayStatics::GetAllActorsOfClass(World, AMassEnemySpawner::StaticClass(), FoundSpawners);

	if (FoundSpawners.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: No spawners found"));
		return 0;
	}

	AMassEnemySpawner* Spawner = Cast<AMassEnemySpawner>(FoundSpawners[0]);
	if (!Spawner)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] FAILED: Spawner cast failed"));
		return 0;
	}

	const TArray<FMassEntityHandle>& SpawnedEntities = Spawner->GetSpawnedEntities();
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Checking %d spawned entities for area damage"), SpawnedEntities.Num());

	int32 ValidCount = 0;
	int32 AliveCount = 0;

	for (const FMassEntityHandle& Handle : SpawnedEntities)
	{
		if (!EntityManager.IsEntityValid(Handle))
		{
			continue;
		}
		ValidCount++;

		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Handle);
		FEnemyStateFragment* State = EntityManager.GetFragmentDataPtr<FEnemyStateFragment>(Handle);
		const FEnemyNetworkFragment* Network = EntityManager.GetFragmentDataPtr<FEnemyNetworkFragment>(Handle);

		if (!Transform || !State || !State->bIsAlive)
		{
			continue;
		}
		AliveCount++;

		FVector EntityLocation = Transform->GetTransform().GetLocation();
		float DistanceSquared = FVector::DistSquared(HitLocation, EntityLocation);
		float Distance = FMath::Sqrt(DistanceSquared);

		int32 NetworkID = Network ? Network->NetworkID : INDEX_NONE;

		if (DistanceSquared <= DamageRadiusSquared)
		{
			State->Health -= Damage;
			DamagedCount++;

			UE_LOG(LogTemp, Warning, TEXT("[MassDamage] HIT! Entity NetworkID %d at distance %.1f, Health: %.1f/%.1f"),
				NetworkID, Distance, State->Health, State->MaxHealth);

			if (State->Health <= 0.0f)
			{
				State->Health = 0.0f;
				State->bIsAlive = false;
				UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Entity NetworkID %d killed by area damage!"), NetworkID);

				if (NetworkID != INDEX_NONE)
				{
					EntitiesToDestroy.Add(NetworkID);
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] Area damage result: Valid=%d, Alive=%d, Damaged=%d"),
		ValidCount, AliveCount, DamagedCount);

	// Destroy killed entities
	for (int32 NetworkID : EntitiesToDestroy)
	{
		DestroyMassEntity(WorldContext, NetworkID);
	}

	return DamagedCount;
}

bool UAutoAimHelper::DestroyMassEntity(AActor* WorldContext, int32 TargetNetworkID)
{
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity called - NetworkID: %d"), TargetNetworkID);

	if (!WorldContext || !WorldContext->GetWorld() || TargetNetworkID == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity FAILED: Invalid WorldContext or NetworkID"));
		return false;
	}

	UWorld* World = WorldContext->GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity FAILED: No EntitySubsystem"));
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Find spawner to get entity handle and remove from tracked list
	TArray<AActor*> FoundSpawners;
	UGameplayStatics::GetAllActorsOfClass(World, AMassEnemySpawner::StaticClass(), FoundSpawners);

	if (FoundSpawners.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity FAILED: No spawners found"));
		return false;
	}

	AMassEnemySpawner* Spawner = Cast<AMassEnemySpawner>(FoundSpawners[0]);
	if (!Spawner)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity FAILED: Spawner cast failed"));
		return false;
	}

	// Find and destroy the entity
	const TArray<FMassEntityHandle>& SpawnedEntities = Spawner->GetSpawnedEntities();
	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity: Searching %d entities for NetworkID %d"),
		SpawnedEntities.Num(), TargetNetworkID);

	for (const FMassEntityHandle& Handle : SpawnedEntities)
	{
		if (!EntityManager.IsEntityValid(Handle))
		{
			continue;
		}

		const FEnemyNetworkFragment* Network = EntityManager.GetFragmentDataPtr<FEnemyNetworkFragment>(Handle);
		if (Network && Network->NetworkID == TargetNetworkID)
		{
			UE_LOG(LogTemp, Error, TEXT("[MassDamage] ===== DESTROYING ENTITY NetworkID %d ====="), TargetNetworkID);

			// Get visualization fragment info BEFORE cleanup for logging
			const FEnemyVisualizationFragment* VisFragmentBefore = EntityManager.GetFragmentDataPtr<FEnemyVisualizationFragment>(Handle);
			if (VisFragmentBefore)
			{
				UE_LOG(LogTemp, Error, TEXT("[MassDamage] BEFORE cleanup - ISMIndex: %d, SkeletalIndex: %d, RenderMode: %d, bISMIsWalking: %s"),
					VisFragmentBefore->ISMInstanceIndex,
					VisFragmentBefore->SkeletalMeshPoolIndex,
					(int32)VisFragmentBefore->RenderMode,
					VisFragmentBefore->bISMIsWalking ? TEXT("true") : TEXT("false"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MassDamage] BEFORE cleanup - NO VisualizationFragment found!"));
			}

			// Clean up visualization BEFORE destroying entity
			// This releases ISM instances and skeletal mesh pool entries so they don't stay visible
			// Use GetInstanceForWorld to get the correct processor for THIS world (handles multiple PIE worlds)
			UEnemyVisualizationProcessor* VisProcessor = UEnemyVisualizationProcessor::GetInstanceForWorld(World);
			UE_LOG(LogTemp, Error, TEXT("[MassDamage] VisProcessor for world %s: %s"),
				*World->GetName(), VisProcessor ? TEXT("VALID") : TEXT("NULL"));

			if (VisProcessor)
			{
				VisProcessor->CleanupEntityVisualization(Handle, EntityManager);

				// Verify cleanup worked
				const FEnemyVisualizationFragment* VisFragmentAfter = EntityManager.GetFragmentDataPtr<FEnemyVisualizationFragment>(Handle);
				if (VisFragmentAfter)
				{
					UE_LOG(LogTemp, Error, TEXT("[MassDamage] AFTER cleanup - ISMIndex: %d, SkeletalIndex: %d, RenderMode: %d"),
						VisFragmentAfter->ISMInstanceIndex,
						VisFragmentAfter->SkeletalMeshPoolIndex,
						(int32)VisFragmentAfter->RenderMode);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MassDamage] ERROR: No VisualizationProcessor instance for cleanup!"));
			}

			// Queue death notification to clients BEFORE destroying entity
			// This ensures clients know to remove this entity from their visualization
			if (UMassEnemyReplicationSubsystem* RepSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>())
			{
				RepSubsystem->QueueDeathNotification(TargetNetworkID);
			}

			// Use deferred destruction to avoid crash during Mass processing
			// Check if Mass is currently processing - if so, defer; otherwise destroy immediately
			if (EntityManager.IsProcessing())
			{
				EntityManager.Defer().DestroyEntity(Handle);
				UE_LOG(LogTemp, Warning, TEXT("[MassDamage] SUCCESS: Deferred destruction of entity NetworkID %d"), TargetNetworkID);
			}
			else
			{
				EntityManager.DestroyEntity(Handle);
				UE_LOG(LogTemp, Warning, TEXT("[MassDamage] SUCCESS: Destroyed entity NetworkID %d"), TargetNetworkID);
			}
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[MassDamage] DestroyMassEntity FAILED: Entity not found"));
	return false;
}

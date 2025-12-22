// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySpawner.h"
#include "MassEntitySubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassCommonFragments.h"
#include "EnemyFragments.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// Create a visual representation in the editor
	#if WITH_EDITORONLY_DATA
		bIsSpatiallyLoaded = false;
	#endif
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSpawnOnBeginPlay)
	{
		SpawnEnemies();
	}
}

void AEnemySpawner::SpawnEnemies()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemySpawner: No valid world"));
		return;
	}

	if (!EnemyEntityConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemySpawner: No EnemyEntityConfig assigned! Please create a MassEntityConfigAsset and assign it."));
		return;
	}

	// Get Mass Entity Subsystem
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemySpawner: MassEntitySubsystem not found. Ensure Mass plugins are enabled."));
		return;
	}

	// Clear any previously spawned entities
	DespawnAllEnemies();

	// Get entity template from config
	const FMassEntityTemplate& EntityTemplate = EnemyEntityConfig->GetOrCreateEntityTemplate(*World);
	const FMassArchetypeHandle Archetype = EntityTemplate.GetArchetype();

	// Reserve space for performance
	SpawnedEntities.Reserve(NumEnemiesToSpawn);

	const FVector SpawnCenter = GetActorLocation() + SpawnCenterOffset;
	
	UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Spawning %d enemies at %s with radius %.1f"), 
		NumEnemiesToSpawn, *SpawnCenter.ToString(), SpawnRadius);

	int32 SuccessfulSpawns = 0;

	// Get entity manager for creation
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Spawn all enemies
	for (int32 i = 0; i < NumEnemiesToSpawn; ++i)
	{
		// Generate random spawn position
		const FVector2D RandomCircle = FMath::RandPointInCircle(SpawnRadius);
		const FVector SpawnLocation = SpawnCenter + FVector(RandomCircle.X, RandomCircle.Y, SpawnHeightOffset);
		const FRotator SpawnRotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

		// Create entity from archetype
		const FMassEntityHandle NewEntity = EntityManager.CreateEntity(Archetype);
		
		if (!NewEntity.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: Failed to create entity %d"), i);
			continue;
		}

		// Set initial transform
		if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(NewEntity))
		{
			FTransform InitialTransform;
			InitialTransform.SetLocation(SpawnLocation);
			InitialTransform.SetRotation(SpawnRotation.Quaternion());
			InitialTransform.SetScale3D(FVector(1.0f));
			TransformFragment->SetTransform(InitialTransform);
		}

		// Initialize movement fragment (now includes velocity)
		if (FEnemyMovementFragment* MovementFragment = EntityManager.GetFragmentDataPtr<FEnemyMovementFragment>(NewEntity))
		{
			MovementFragment->Velocity = FVector::ZeroVector;
			MovementFragment->FacingDirection = SpawnRotation.Vector();
		}

		// Initialize state fragment with unique ID
		if (FEnemyStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FEnemyStateFragment>(NewEntity))
		{
			StateFragment->EntityID = i;
			StateFragment->PreviousLocation = SpawnLocation;
			StateFragment->bIsAlive = true;
		}

		// Track spawned entity
		SpawnedEntities.Add(NewEntity);
		SuccessfulSpawns++;
	}

	UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Successfully spawned %d/%d enemies using Mass Entity System"), 
		SuccessfulSpawns, NumEnemiesToSpawn);
}

void AEnemySpawner::DespawnAllEnemies()
{
	if (SpawnedEntities.Num() == 0)
		return;

	UWorld* World = GetWorld();
	if (!World) return;

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	int32 DespawnedCount = 0;

	// Destroy all spawned entities
	for (const FMassEntityHandle& Entity : SpawnedEntities)
	{
		if (Entity.IsValid())
		{
			EntityManager.DestroyEntity(Entity);
			DespawnedCount++;
		}
	}

	SpawnedEntities.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Despawned %d enemies"), DespawnedCount);
}

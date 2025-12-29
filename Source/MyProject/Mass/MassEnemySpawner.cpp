// Fill out your copyright notice in the Description page of Project Settings.

#include "MassEnemySpawner.h"
#include "MassEntitySubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "EnemyFragments.h"
#include "UObject/ConstructorHelpers.h"

AMassEnemySpawner::AMassEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// Create root component for spawner placement
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	
	// Initialize cached template pointer
	CachedEntityTemplate = nullptr;
	
	// Load entity config asset
	static ConstructorHelpers::FObjectFinder<UMassEntityConfigAsset> EntityConfigAsset(
		TEXT("/Script/MassSpawner.MassEntityConfigAsset'/Game/DA_Enemy.DA_Enemy'")
	);
	if (EntityConfigAsset.Succeeded())
	{
		EnemyEntityConfig = EntityConfigAsset.Object;
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Entity config loaded successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Failed to load DA_Enemy - assign manually in editor"));
	}
}

void AMassEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: BeginPlay called"));

	// Only spawn on server - clients will receive replicated entities
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Running on client - skipping spawn initialization"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Running on server - initializing spawner"));

	// Validation
	if (!EnemyEntityConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEnemySpawner: EnemyEntityConfig not assigned! Please set DA_EnemyEntity."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEnemySpawner: No valid world"));
		return;
	}

	// Verify Mass subsystem exists
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEnemySpawner: MassEntitySubsystem not found! Enable Mass plugins."));
		return;
	}

	// Cache entity template for performance
	CachedEntityTemplate = &EnemyEntityConfig->GetOrCreateEntityTemplate(*World);

	// Setup optional visualization
	SetupVisualization();

	// Auto-start spawning if enabled
	if (bAutoSpawnOnBeginPlay)
	{
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Auto-starting spawn waves"));
		StartSpawning();
	}

	UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Initialized successfully"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void AMassEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop spawning first
	StopSpawning();
	
	// Clear ISM first to prevent rendering updates on destroyed components
	if (VisualizationISM && VisualizationISM->IsValidLowLevel() && !VisualizationISM->IsUnreachable())
	{
		// Don't call MarkRenderStateDirty during EndPlay - can cause assertion failures
		if (VisualizationISM->IsRegistered())
		{
			VisualizationISM->ClearInstances();
		}
	}
	
	// Then destroy entities
	if (UWorld* World = GetWorld())
	{
		if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
		{
			FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
			
			if (SpawnedEntities.Num() > 0)
			{
				TArray<FMassEntityHandle> EntitiesToDestroy;
				EntitiesToDestroy.Reserve(SpawnedEntities.Num());
				
				for (const FMassEntityHandle& Entity : SpawnedEntities)
				{
					if (EntityManager.IsEntityValid(Entity))
					{
						EntitiesToDestroy.Add(Entity);
					}
				}
				
				if (EntitiesToDestroy.Num() > 0)
				{
					EntityManager.BatchDestroyEntities(EntitiesToDestroy);
				}
			}
			
			SpawnedEntities.Empty();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMassEnemySpawner::SetupVisualization()
{
	// Optional: Create ISM for visualization
	// Note: EnemyVisualizationProcessor can also handle this
	
	if (!EnemyMesh)
	{
		// No mesh set, processor will handle visualization
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: No mesh set - processor will handle visualization"));
		return;
	}

	VisualizationISM = NewObject<UInstancedStaticMeshComponent>(
		this,
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("MassEnemyVisualizationISM")
	);
	
	if (VisualizationISM)
	{
		VisualizationISM->RegisterComponent();
		VisualizationISM->AttachToComponent(
			RootComponent, 
			FAttachmentTransformRules::KeepRelativeTransform
		);
		
		// Configure mesh
		VisualizationISM->SetStaticMesh(EnemyMesh);
		
		if (EnemyMaterial)
		{
			VisualizationISM->SetMaterial(0, EnemyMaterial);
		}
		
		// Performance settings
		VisualizationISM->SetCastShadow(false);
		VisualizationISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VisualizationISM->SetCanEverAffectNavigation(false);
		VisualizationISM->SetGenerateOverlapEvents(false);
		
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Visualization ISM created"));
	}
}

void AMassEnemySpawner::StartSpawning()
{
	// Only spawn on server
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: StartSpawning called on client - ignoring"));
		return;
	}

	if (!EnemyEntityConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEnemySpawner: Cannot start spawning - no config set"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Stop existing timer if running
	if (SpawnTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Spawning single wave of 30 enemies"));

	// Spawn one wave with 30 enemies
	const int32 OriginalEnemiesPerWave = EnemiesPerWave;
	EnemiesPerWave = 20;
	SpawnWave();
	EnemiesPerWave = OriginalEnemiesPerWave;

	UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Single wave spawn complete"));
}

void AMassEnemySpawner::StopSpawning()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (SpawnTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		SpawnTimerHandle.Invalidate();
		
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Stopped wave spawning"));
	}
}

void AMassEnemySpawner::SpawnWave()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("MassEnemySpawner::SpawnWave"));

	// Only spawn on server
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: SpawnWave called on client - ignoring"));
		return;
	}

	// Cleanup invalid entities first
	CleanupInvalidEntities();

	// Check max enemies limit (like original ActiveEnemyCount check)
	const int32 CurrentCount = GetActiveEnemyCount();
	if (CurrentCount >= MaxEnemies)
	{
		UE_LOG(LogTemp, Verbose, TEXT("MassEnemySpawner: Max enemies reached (%d/%d)"), 
			CurrentCount, MaxEnemies);
		return;
	}

	int32 SpawnedThisWave = 0;
	int32 FailedSpawns = 0;

	// Spawn enemies for this wave
	for (int32 i = 0; i < EnemiesPerWave; ++i)
	{
		// Check limit before each spawn
		if (GetActiveEnemyCount() >= MaxEnemies)
		{
			break;
		}

		// Get random spawn location
		FVector SpawnLocation;
		if (GetSpawnLocation(SpawnLocation))
		{
			FMassEntityHandle NewEnemy = CreateMassEntity(SpawnLocation);
			
			UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
			if (EntitySubsystem && EntitySubsystem->GetEntityManager().IsEntityValid(NewEnemy))
			{
				SpawnedThisWave++;
			}
			else
			{
				FailedSpawns++;
			}
		}
		else
		{
			FailedSpawns++;
		}
	}

	if (SpawnedThisWave > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Wave complete - Spawned: %d, Failed: %d, Total: %d/%d"), 
			SpawnedThisWave, FailedSpawns, GetActiveEnemyCount(), MaxEnemies);
	}
}

void AMassEnemySpawner::SpawnSingleEnemy(const FVector& Location)
{
	// Only spawn on server
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: SpawnSingleEnemy called on client - ignoring"));
		return;
	}

	if (GetActiveEnemyCount() >= MaxEnemies)
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Cannot spawn - max limit reached"));
		return;
	}

	FMassEntityHandle NewEnemy = CreateMassEntity(Location);
	
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem && EntitySubsystem->GetEntityManager().IsEntityValid(NewEnemy))
	{
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Single enemy spawned at %s"), 
			*Location.ToString());
	}
}

FMassEntityHandle AMassEnemySpawner::CreateMassEntity(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) return FMassEntityHandle();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return FMassEntityHandle();

	if (!CachedEntityTemplate)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEnemySpawner: No cached entity template"));
		return FMassEntityHandle();
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	
	// Create entity with archetype structure
	FMassEntityHandle NewEntity = EntityManager.CreateEntity(CachedEntityTemplate->GetArchetype());
	
	if (!EntityManager.IsEntityValid(NewEntity))
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Failed to create entity"));
		return FMassEntityHandle();
	}

	// ========== SAFE INITIALIZATION WITH DEBUG LOGGING ==========
	UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: Initializing entity fragments..."));
	
	// 1. Transform Fragment (always exists)
	if (FTransformFragment* TransformFragment = 
		EntityManager.GetFragmentDataPtr<FTransformFragment>(NewEntity))
	{
		FTransform InitialTransform;
		InitialTransform.SetLocation(Location);
		InitialTransform.SetRotation(FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f).Quaternion());
		InitialTransform.SetScale3D(FVector(1.0f));
		TransformFragment->SetTransform(InitialTransform);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MISSING: FTransformFragment"));
	}

	// 2. State Fragment
	if (FEnemyStateFragment* StateFragment =
		EntityManager.GetFragmentDataPtr<FEnemyStateFragment>(NewEntity))
	{
		StateFragment->bIsAlive = true;
		StateFragment->bIsMoving = false;
		StateFragment->PreviousLocation = Location;
		StateFragment->Health = 100.0f;
		StateFragment->MaxHealth = 100.0f;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MISSING: FEnemyStateFragment"));
	}

	// 3. Movement Fragment (now includes Velocity)
	if (FEnemyMovementFragment* MovementFragment = 
    EntityManager.GetFragmentDataPtr<FEnemyMovementFragment>(NewEntity))
{
    MovementFragment->Velocity = FVector::ZeroVector;
    MovementFragment->Acceleration = FVector::ZeroVector;
    MovementFragment->FacingDirection = FVector::ForwardVector;
    MovementFragment->MaxSpeed = 600.0f;
    MovementFragment->MovementSpeed = 250.0f;
    MovementFragment->RotationSpeed = 10.0f;
    MovementFragment->AcceptanceRadius = 30.0f;
    MovementFragment->PathUpdateInterval = 0.2f;
    MovementFragment->TimeSinceLastPathUpdate = 0.0f;
    MovementFragment->bIsFalling = false;  // NEW - required for animation
}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MISSING: FEnemyMovementFragment"));
	}

	// 5. Attack Fragment
	if (FEnemyAttackFragment* AttackFragment = 
    EntityManager.GetFragmentDataPtr<FEnemyAttackFragment>(NewEntity))
{
    AttackFragment->AttackRange = 150.0f;
    AttackFragment->AttackInterval = 1.5f;
    AttackFragment->AttackDamage = 0.5f;
    AttackFragment->TimeSinceLastAttack = 0.0f;
    AttackFragment->bIsInAttackRange = false;
    // New fields for visualization
    AttackFragment->bIsAttacking = false;
    AttackFragment->AttackType = 0;
    AttackFragment->bHitPending = false;
    AttackFragment->HitDirection = 0.0f;
    AttackFragment->LookAtTarget = FVector::ZeroVector;
    AttackFragment->bHasLookAtTarget = false;
}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MISSING: FEnemyAttackFragment"));
	}

	// 6. Target Fragment
	if (FEnemyTargetFragment* TargetFragment = 
		EntityManager.GetFragmentDataPtr<FEnemyTargetFragment>(NewEntity))
	{
		TargetFragment->TargetLocation = FVector::ZeroVector;
		TargetFragment->TargetActor = nullptr;
		TargetFragment->DistanceToTarget = 0.0f;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MISSING: FEnemyTargetFragment"));
	}

	// 7. Visualization Fragment
if (FEnemyVisualizationFragment* VisFragment = 
    EntityManager.GetFragmentDataPtr<FEnemyVisualizationFragment>(NewEntity))
{
    VisFragment->RenderMode = EEnemyRenderMode::None;  // Processor will set this
    VisFragment->ISMInstanceIndex = INDEX_NONE;
    VisFragment->SkeletalMeshPoolIndex = INDEX_NONE;
    VisFragment->bIsVisible = true;
    VisFragment->CurrentLOD = 0;
    VisFragment->CachedDistanceToCamera = 0.0f;
    VisFragment->AnimationTime = 0.0f;
    VisFragment->AnimationPlayRate = 1.0f;
}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MISSING: FEnemyVisualizationFragment"));
	}

	// Track spawned entity
	SpawnedEntities.Add(NewEntity);

	UE_LOG(LogTemp, Warning, TEXT("Entity created successfully at %s"), *Location.ToString());

	return NewEntity;
}

bool AMassEnemySpawner::GetSpawnLocation(FVector& OutLocation)
{
	// Get player pawn (cached for performance)
	APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		return false;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	// Try multiple times to find valid spawn point
	const int32 MaxAttempts = 5;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Generate random angle and distance
		const float RandomAngle = FMath::RandRange(0.0f, 360.0f);
		const float RandomDistance = FMath::RandRange(MinSpawnDistance, SpawnRadius);
		
		// Calculate spawn position
		const FVector Offset = FVector(
			FMath::Cos(FMath::DegreesToRadians(RandomAngle)) * RandomDistance,
			FMath::Sin(FMath::DegreesToRadians(RandomAngle)) * RandomDistance,
			0.0f
		);
		
		const FVector ProposedLocation = PlayerLocation + Offset;

		// Use navigation system if enabled (like original spawner)
		if (bUseNavigationSystem)
		{
			UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
			if (NavSys)
			{
				FNavLocation NavLocation;
				if (NavSys->ProjectPointToNavigation(
					ProposedLocation, 
					NavLocation, 
					FVector(500.0f, 500.0f, 500.0f)))  // Search extent
				{
					OutLocation = NavLocation.Location + FVector(0, 0, 100);  // Offset like original
					return true;
				}
			}
		}
		else
		{
			// No navigation system - use proposed location directly
			OutLocation = ProposedLocation + FVector(0, 0, 100);
			return true;
		}
	}

	// Fallback: spawn at random point without validation
	const FVector2D RandomCircle = FMath::RandPointInCircle(SpawnRadius);
	OutLocation = PlayerLocation + FVector(RandomCircle.X, RandomCircle.Y, 100.0f);
	return true;
}

APawn* AMassEnemySpawner::GetPlayerPawn()
{
	// Use cached reference if valid
	if (CachedPlayerPawn.IsValid())
	{
		return CachedPlayerPawn.Get();
	}

	// Get player pawn (like original spawner)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		CachedPlayerPawn = PlayerPawn;
		return PlayerPawn;
	}

	return nullptr;
}

void AMassEnemySpawner::CleanupInvalidEntities()
{
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	
	// Remove invalid entities from tracking array
	// This replaces the OnEnemyDestroyed callback in original spawner
	SpawnedEntities.RemoveAll([&EntityManager](const FMassEntityHandle& Entity)
	{
		return !EntityManager.IsEntityValid(Entity);
	});
}

void AMassEnemySpawner::DespawnAllEnemies()
{
	// Only despawn on server
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("MassEnemySpawner: DespawnAllEnemies called on client - ignoring"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const int32 CountBeforeDespawn = SpawnedEntities.Num();

	// Clear ISM instances FIRST before destroying entities to avoid dangling references
	// Only mark dirty if world is not tearing down
	if (VisualizationISM && VisualizationISM->IsValidLowLevel() && !VisualizationISM->IsUnreachable())
	{
		if (VisualizationISM->IsRegistered())
		{
			VisualizationISM->ClearInstances();
			
			// Only mark dirty if world is still valid
			if (!World->bIsTearingDown)
			{
				VisualizationISM->MarkRenderStateDirty();
			}
		}
	}

	// Batch destroy entities for better performance and safety
	if (SpawnedEntities.Num() > 0)
	{
		TArray<FMassEntityHandle> EntitiesToDestroy;
		EntitiesToDestroy.Reserve(SpawnedEntities.Num());
		
		for (const FMassEntityHandle& Entity : SpawnedEntities)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				EntitiesToDestroy.Add(Entity);
			}
		}
		
		// Batch destroy is safer than individual destroys
		if (EntitiesToDestroy.Num() > 0)
		{
			EntityManager.BatchDestroyEntities(EntitiesToDestroy);
		}
	}

	SpawnedEntities.Empty();
	
	if (CountBeforeDespawn > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("MassEnemySpawner: Despawned %d enemies"), CountBeforeDespawn);
	}
}

bool AMassEnemySpawner::IsSpawning() const
{
	return SpawnTimerHandle.IsValid();
}

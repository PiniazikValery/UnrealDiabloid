// EnemyVisualizationProcessor.cpp
// Hybrid visualization: Skeletal Mesh + ISM/VAT

#include "EnemyVisualizationProcessor.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineUtils.h" // For TActorIterator
#include "Engine/LocalPlayer.h" // For player index retrieval

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UEnemyVisualizationProcessor::UEnemyVisualizationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	SkeletalMeshMaxDistance = 2000.0f;
	VATMaxDistance = 5000.0f;
	LODHysteresis = 50.0f;
	SkeletalMeshPoolSize = 10;
	UpdateFrequency = 1;
	bCastShadows = false;
	bEnableVATRendering = true;

	// ISM velocity hysteresis to prevent flickering when switching idle/walk
	ISMVelocityThreshold = 10.0f;
	ISMVelocityHysteresis = 5.0f;

	// Animation sync settings for smooth LOD transitions
	bEnableAnimationSync = true;
	IdleAnimationCycleDuration = 2.0f;
	WalkAnimationCycleDuration = 0.8f;
	AnimationSyncTolerance = 0.15f;
	MaxSyncWaitTime = 1.0f;
	// Enemies now get skeletal mesh based on slot assignment, not movement state
	// Even idle enemies get skeletal mesh if they're assigned to follow a nearby player
	bSkipSkeletalMeshForIdleEnemies = false;

	// Skeletal mesh - remove the "/Script/Engine.SkeletalMesh'" prefix
	EnemySkeletalMesh = TSoftObjectPtr<USkeletalMesh>(
		FSoftObjectPath(TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Enemies/Skeleton/scene/SkeletonSM.SkeletonSM'")));

	// Animation BP - needs _C suffix for the generated class
	AnimationInstanceClass = TSoftClassPtr<UEnemyAnimInstance>(
		FSoftObjectPath(TEXT("/Game/Characters/Enemies/Skeleton/scene/Animations/EABP.EABP_C")));

	// Use the mannequin static mesh for distant enemies (for VAT or simple ISM)
	SimpleDistantMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Script/Engine.StaticMesh'/Game/Characters/Enemies/Skeleton/scene/ISM/StaticMesh.StaticMesh'")));

	// Material instances for ISM - Element 0 (Skeleton_Material_0)
	ISM_Material_0_Idle = TSoftObjectPtr<UMaterialInstance>(
		FSoftObjectPath(TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Characters/Enemies/Skeleton/scene/ISM/Skeleton_Material_0_Inst_Idle.Skeleton_Material_0_Inst_Idle'")));
	ISM_Material_0_Walk = TSoftObjectPtr<UMaterialInstance>(
		FSoftObjectPath(TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Characters/Enemies/Skeleton/scene/ISM/Skeleton_Material_0_Inst_Walk.Skeleton_Material_0_Inst_Walk'")));

	// Material instances for ISM - Element 1 (Skeleton_Material)
	ISM_Material_1_Idle = TSoftObjectPtr<UMaterialInstance>(
		FSoftObjectPath(TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Characters/Enemies/Skeleton/scene/ISM/Skeleton_Material_Inst_Idle.Skeleton_Material_Inst_Idle'")));
	ISM_Material_1_Walk = TSoftObjectPtr<UMaterialInstance>(
		FSoftObjectPath(TEXT("/Script/Engine.MaterialInstanceConstant'/Game/Characters/Enemies/Skeleton/scene/ISM/Skeleton_Material_Inst_Walk.Skeleton_Material_Inst_Walk'")));

#if PLATFORM_ANDROID || PLATFORM_IOS
	// Aggressive mobile optimizations for better performance
	SkeletalMeshPoolSize = 3;		  // Reduced from 20 - fewer animated meshes
	SkeletalMeshMaxDistance = 300.0f; // Reduced from 1000.0f - show skeletal meshes only very close
	VATMaxDistance = 2000.0f;		  // Reduced from 5000.0f - cull distant enemies sooner
	UpdateFrequency = 2;			  // Increased from 2 - update less frequently (every 3rd frame)
	bCastShadows = false;			  // Ensure shadows are off on mobile
#endif

	UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Constructed"));
	bRequiresGameThreadExecution = true;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UEnemyVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEnemyVisualizationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEnemyAttackFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FEnemyStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEnemyTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FEnemyNetworkFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FEnemyTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FEnemyDeadTag>(EMassFragmentPresence::None);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UEnemyVisualizationProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UWorld* World = Owner.GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyVisualizationProcessor: No valid world"));
		return;
	}

	// Only initialize visualization in game worlds (PIE or standalone), not in the editor world
	if (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Skipping initialization in editor world (WorldType=%d)"), (int32)World->WorldType);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("InitializeInternal called"));

#if PLATFORM_ANDROID || PLATFORM_IOS
	// Apply mobile rendering optimizations
	if (IConsoleVariable* ScreenPercentageCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		ScreenPercentageCVar->Set(70.0f); // Render at 70% resolution for better performance
		UE_LOG(LogTemp, Log, TEXT("Mobile: Set r.ScreenPercentage to 70"));
	}

	if (IConsoleVariable* ContentScaleCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor")))
	{
		ContentScaleCVar->Set(0.7f); // Scale content to 70%
		UE_LOG(LogTemp, Log, TEXT("Mobile: Set r.MobileContentScaleFactor to 0.7"));
	}
#endif

	LoadAssets();
	InitializeSkeletalMeshPool(World);
	InitializeVATSystem(World);

	bIsInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Initialization complete"));
}

void UEnemyVisualizationProcessor::LoadAssets()
{
	UE_LOG(LogTemp, Log, TEXT("LoadAssets called"));
	if (!EnemySkeletalMesh.IsNull())
	{
		EnemySkeletalMesh.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Skeletal mesh loaded"));
	}

	if (!AnimationInstanceClass.IsNull())
	{
		AnimationInstanceClass.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Animation class loaded"));
	}

	if (!VATConfig.VATStaticMesh.IsNull())
	{
		VATConfig.VATStaticMesh.LoadSynchronous();
	}

	if (!VATConfig.VATMaterial.IsNull())
	{
		VATConfig.VATMaterial.LoadSynchronous();
	}

	if (!VATConfig.PositionTexture.IsNull())
	{
		VATConfig.PositionTexture.LoadSynchronous();
	}

	if (!VATConfig.NormalTexture.IsNull())
	{
		VATConfig.NormalTexture.LoadSynchronous();
	}

	// Load ISM material instances for idle/walk states
	if (!ISM_Material_0_Idle.IsNull())
	{
		ISM_Material_0_Idle.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: ISM_Material_0_Idle loaded"));
	}
	if (!ISM_Material_0_Walk.IsNull())
	{
		ISM_Material_0_Walk.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: ISM_Material_0_Walk loaded"));
	}
	if (!ISM_Material_1_Idle.IsNull())
	{
		ISM_Material_1_Idle.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: ISM_Material_1_Idle loaded"));
	}
	if (!ISM_Material_1_Walk.IsNull())
	{
		ISM_Material_1_Walk.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: ISM_Material_1_Walk loaded"));
	}

	UE_LOG(LogTemp, Error, TEXT("=== LOADED ASSETS CHECK ==="));
	UE_LOG(LogTemp, Error, TEXT("SimpleDistantMesh: %s"),
		SimpleDistantMesh.Get() ? *SimpleDistantMesh.Get()->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Error, TEXT("ISM_Material_0_Idle: %s"),
		ISM_Material_0_Idle.Get() ? *ISM_Material_0_Idle.Get()->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Error, TEXT("ISM_Material_1_Idle: %s"),
		ISM_Material_1_Idle.Get() ? *ISM_Material_1_Idle.Get()->GetName() : TEXT("NULL"));
}

void UEnemyVisualizationProcessor::InitializeSkeletalMeshPool(UWorld* World)
{
	USkeletalMesh* LoadedMesh = EnemySkeletalMesh.Get();
	UClass*		   AnimClass = AnimationInstanceClass.Get();

	UE_LOG(LogTemp, Warning, TEXT("=== InitializeSkeletalMeshPool ==="));
	UE_LOG(LogTemp, Warning, TEXT("LoadedMesh: %s"), LoadedMesh ? *LoadedMesh->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("AnimClass: %s"), AnimClass ? *AnimClass->GetName() : TEXT("NULL"));

	if (!LoadedMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyVisualizationProcessor: No skeletal mesh! EnemySkeletalMesh=%s"),
			*EnemySkeletalMesh.ToString());
		return;
	}

	// Cleanup any stale actors from previous runs (they shouldn't exist if RF_Transient is working)
#if WITH_EDITOR
	TArray<AActor*> StaleActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetActorLabel().StartsWith(TEXT("EnemySkelMesh_")))
		{
			StaleActors.Add(Actor);
		}
	}
	for (AActor* StaleActor : StaleActors)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cleaning up stale skeletal mesh actor: %s"), *StaleActor->GetActorLabel());
		StaleActor->Destroy();
	}
#endif

	SkeletalMeshPool.Reserve(SkeletalMeshPoolSize);
	FreeSkeletalMeshIndices.Reserve(SkeletalMeshPoolSize);

	for (int32 i = 0; i < SkeletalMeshPoolSize; ++i)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient; // Don't save to level

		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0, 0, -10000), FRotator::ZeroRotator, SpawnParams);

		if (!Actor)
		{
			continue;
		}

#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("EnemySkelMesh_%d"), i));
#endif

		// Create the skeletal mesh component as the ROOT component
		USkeletalMeshComponent* SkelMeshComp = NewObject<USkeletalMeshComponent>(
			Actor,
			USkeletalMeshComponent::StaticClass(),
			*FString::Printf(TEXT("SkelMesh_%d"), i));

		if (!SkelMeshComp)
		{
			Actor->Destroy();
			continue;
		}

		// Set as root component BEFORE registering
		Actor->SetRootComponent(SkelMeshComp);
		SkelMeshComp->RegisterComponent();
		SkelMeshComp->SetSkeletalMesh(LoadedMesh);

		// Set custom animation instance class
		if (AnimClass)
		{
			SkelMeshComp->SetAnimInstanceClass(AnimClass);
		}

		// Optimize for pooling
		SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkelMeshComp->SetCastShadow(bCastShadows);
		// CRITICAL FIX: Always tick animations, don't tie to visibility
		SkelMeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		SkelMeshComp->SetComponentTickEnabled(false);

#if PLATFORM_ANDROID || PLATFORM_IOS
		// Mobile-specific optimizations
		SkelMeshComp->OverrideMinLOD(1); // Skip highest LOD entirely
		SkelMeshComp->AnimUpdateRateParams->BaseNonRenderedUpdateRate = 8;
		SkelMeshComp->bEnableUpdateRateOptimizations = true;
#endif

		// Start hidden
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);

		// Add to pool
		FSkeletalMeshPoolEntry Entry;
		Entry.Actor = Actor;
		Entry.SkeletalMeshComponent = SkelMeshComp;
		Entry.AnimInstance = Cast<UEnemyAnimInstance>(SkelMeshComp->GetAnimInstance());
		Entry.bInUse = false;

		int32 PoolIndex = SkeletalMeshPool.Add(Entry);
		FreeSkeletalMeshIndices.Add(PoolIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Created skeletal mesh pool with %d entries"),
		SkeletalMeshPool.Num());
}

void UEnemyVisualizationProcessor::InitializeVATSystem(UWorld* World)
{
	// STEP 1: Load mesh
	UStaticMesh* VATMesh = VATConfig.VATStaticMesh.Get();

	if (!VATMesh)
	{
		VATMesh = SimpleDistantMesh.LoadSynchronous();
	}

	if (!VATMesh)
	{
		VATMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UE_LOG(LogTemp, Error, TEXT("ISM: Falling back to cube!"));
	}

	if (!VATMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ISM: No mesh available!"));
		return;
	}

	// STEP 2: Create actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	VATVisualizationActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (!VATVisualizationActor)
	{
		UE_LOG(LogTemp, Error, TEXT("ISM: Failed to spawn actor!"));
		return;
	}

#if WITH_EDITOR
	VATVisualizationActor->SetActorLabel(TEXT("EnemyDistantVisualization"));
#endif

	// STEP 3: Create IDLE ISM
	VATISM = NewObject<UInstancedStaticMeshComponent>(
		VATVisualizationActor,
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("DistantEnemyISM_Idle"));

	if (!VATISM)
	{
		UE_LOG(LogTemp, Error, TEXT("ISM: Failed to create Idle component!"));
		return;
	}

	// Setup as root component BEFORE registering
	VATVisualizationActor->SetRootComponent(VATISM);
	VATISM->RegisterComponent();
	VATISM->SetStaticMesh(VATMesh);

	// Materials
	if (UMaterialInstance* Mat0 = ISM_Material_0_Idle.Get())
		VATISM->SetMaterial(0, Mat0);
	if (UMaterialInstance* Mat1 = ISM_Material_1_Idle.Get())
		VATISM->SetMaterial(1, Mat1);

	// Optimize
	VATISM->SetCastShadow(false);
	VATISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VATISM->bDisableCollision = true;
VATISM->SetCullDistances(0, 0);  // Отключить distance culling
VATISM->bNeverDistanceCull = true;
VATISM->bAlwaysCreatePhysicsState = false;
	VATISM->SetCanEverAffectNavigation(false);
	VATISM->SetGenerateOverlapEvents(false);
	VATISM->bUseAsOccluder = false;
	VATISM->NumCustomDataFloats = 0;

	// STEP 4: Create WALK ISM
	VATISM_Walk = NewObject<UInstancedStaticMeshComponent>(
		VATVisualizationActor,
		UInstancedStaticMeshComponent::StaticClass(),
		TEXT("DistantEnemyISM_Walk"));

	if (!VATISM_Walk)
	{
		UE_LOG(LogTemp, Error, TEXT("ISM: Failed to create Walk component!"));
		return;
	}

	VATISM_Walk->RegisterComponent();
	VATISM_Walk->AttachToComponent(VATISM, FAttachmentTransformRules::KeepRelativeTransform);
	VATISM_Walk->SetStaticMesh(VATMesh);

	// Materials
	if (UMaterialInstance* Mat0 = ISM_Material_0_Walk.Get())
		VATISM_Walk->SetMaterial(0, Mat0);
	if (UMaterialInstance* Mat1 = ISM_Material_1_Walk.Get())
		VATISM_Walk->SetMaterial(1, Mat1);

	// Optimize
	VATISM_Walk->SetCastShadow(false);
	VATISM_Walk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VATISM_Walk->bDisableCollision = true;
VATISM_Walk->SetCullDistances(0, 0);
VATISM_Walk->bNeverDistanceCull = true;
VATISM_Walk->bAlwaysCreatePhysicsState = false;
	VATISM_Walk->SetCanEverAffectNavigation(false);
	VATISM_Walk->SetGenerateOverlapEvents(false);
	VATISM_Walk->bUseAsOccluder = false;
	VATISM_Walk->NumCustomDataFloats = 0;

	bEnableVATRendering = true;
}

// ============================================================================
// MAIN EXECUTION
// ============================================================================

void UEnemyVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("EnemyVisualizationProcessor::Execute"));

	UWorld* World = EntityManager.GetWorld();
	if (!World || World->bIsTearingDown || !bIsInitialized)
	{
		return;
	}

	FrameCounter++;
	if (UpdateFrequency > 1 && (FrameCounter % UpdateFrequency) != 0)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	// Periodically refresh the player list to handle players joining/leaving
	if (CurrentTime - LastPlayerRefreshTime >= PlayerRefreshInterval || CachedPlayerPawns.Num() == 0)
	{
		LastPlayerRefreshTime = CurrentTime;
		CachedPlayerPawns.Empty();
		CachedPlayerLocations.Empty();

		// Get all player controllers and cache their pawns
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->GetPawn())
			{
				int32 PlayerIndex = 0;
				// Try to get the player index from the local player
				ULocalPlayer* LP = Cast<ULocalPlayer>(PC->Player);
				if (LP)
				{
					PlayerIndex = LP->GetControllerId();
				}
				else
				{
					// For network players, use the order they appear
					PlayerIndex = CachedPlayerPawns.Num();
				}
				CachedPlayerPawns.Add(PlayerIndex, PC->GetPawn());
				CachedPlayerLocations.Add(PlayerIndex, PC->GetPawn()->GetActorLocation());
			}
		}
	}
	else
	{
		// Quick update of player locations
		for (auto& Pair : CachedPlayerPawns)
		{
			if (Pair.Value.IsValid())
			{
				CachedPlayerLocations.FindOrAdd(Pair.Key) = Pair.Value->GetActorLocation();
			}
		}
	}

	// Validate cached players (some may have died or disconnected)
	for (auto It = CachedPlayerPawns.CreateIterator(); It; ++It)
	{
		if (!It->Value.IsValid())
		{
			CachedPlayerLocations.Remove(It->Key);
			It.RemoveCurrent();
		}
	}

	// Get first player location for fallback/camera reference
	CachedCameraLocation = FVector::ZeroVector;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			CachedCameraLocation = PlayerPawn->GetActorLocation();
		}
		else if (PC->PlayerCameraManager)
		{
			CachedCameraLocation = PC->PlayerCameraManager->GetCameraLocation();
		}
	}

	const float DeltaTime = World->GetDeltaSeconds() * UpdateFrequency;
	const float MaxRenderDistanceSq = VATMaxDistance * VATMaxDistance;

	// Check if we're on a client - on clients, we use replicated data for player assignment
	const bool bIsClient = (World->GetNetMode() == NM_Client);

	// NOTE: On client, we cannot reliably determine our server-assigned player index
	// because the server uses a custom indexing scheme (host gets GetControllerId(),
	// network players get sequential indices).
	//
	// Instead, we use a simpler approach: if an enemy has ANY slot assignment
	// (Network.TargetPlayerIndex >= 0) and is close to the local player,
	// show skeletal mesh. This works because enemies following a specific player
	// will be near that player. If they're near us, they're likely following us.

	// ========== PASS 1: Collect entities and update skeletal meshes ==========
	CachedAllEntities.Reset();
	if (CachedAllEntities.Max() < 1024)
	{
		CachedAllEntities.Reserve(1024);
	}

	int32 TotalAlive = 0;

	EntityQuery.ForEachEntityChunk(Context,
		[this, DeltaTime, MaxRenderDistanceSq, &TotalAlive, bIsClient](FMassExecutionContext& Context) {
			const auto								 TransformList = Context.GetFragmentView<FTransformFragment>();
			auto									 VisualizationList = Context.GetMutableFragmentView<FEnemyVisualizationFragment>();
			const auto								 MovementList = Context.GetFragmentView<FEnemyMovementFragment>();
			auto									 AttackList = Context.GetMutableFragmentView<FEnemyAttackFragment>();
			const auto								 StateList = Context.GetFragmentView<FEnemyStateFragment>();
			const auto								 TargetList = Context.GetFragmentView<FEnemyTargetFragment>();
			const auto								 NetworkList = Context.GetFragmentView<FEnemyNetworkFragment>();
			const TConstArrayView<FMassEntityHandle> Entities = Context.GetEntities();
			const int32								 NumEntities = Context.GetNumEntities();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				if (!StateList[i].bIsAlive)
				{
					continue;
				}

				TotalAlive++;
				FEnemyVisualizationFragment&	VisFragment = VisualizationList[i];
				const FTransform&				Transform = TransformList[i].GetTransform();
				const FEnemyMovementFragment&	Movement = MovementList[i];
				FEnemyAttackFragment&			Attack = AttackList[i];
				const FEnemyStateFragment&		State = StateList[i];
				const FEnemyTargetFragment&		Target = TargetList[i];
				const FEnemyNetworkFragment&	Network = NetworkList[i];

				const FVector EnemyLocation = Transform.GetLocation();

				// Calculate distance to the player this enemy is assigned to follow
				// Only enemies that have a slot assigned to a player should be candidates for skeletal mesh
				float DistanceToAssignedPlayer = FLT_MAX;
				bool bHasAssignedPlayer = false;

				// On server: use Movement.AssignedSlotPlayerIndex (authoritative slot data)
				// On client: if enemy has ANY slot assignment (TargetPlayerIndex >= 0),
				//            check distance to local player. Enemies following a specific player
				//            will be near that player, so if they're near us, they're following us.
				int32 AssignedPlayerIndex = INDEX_NONE;
				if (bIsClient)
				{
					// Client: if enemy has a slot assignment, use local player distance
					// This simplification works because enemies assigned to other players
					// will be near those players, not near us
					if (Network.TargetPlayerIndex >= 0)
					{
						AssignedPlayerIndex = Network.TargetPlayerIndex;
					}
					// else: AssignedPlayerIndex stays INDEX_NONE, DistanceToAssignedPlayer stays FLT_MAX
				}
				else
				{
					// Server uses authoritative slot assignment
					if (Movement.bHasAssignedSlot && Movement.AssignedSlotPlayerIndex != INDEX_NONE)
					{
						AssignedPlayerIndex = Movement.AssignedSlotPlayerIndex;
					}
					else if (Target.TargetPlayerIndex != INDEX_NONE)
					{
						AssignedPlayerIndex = Target.TargetPlayerIndex;
					}
				}

				if (AssignedPlayerIndex != INDEX_NONE)
				{
					// For client, use local player location (CachedCameraLocation)
					// For server, look up the assigned player's location
					if (bIsClient)
					{
						DistanceToAssignedPlayer = FVector::Dist(EnemyLocation, CachedCameraLocation);
						bHasAssignedPlayer = true;
					}
					else
					{
						const FVector* AssignedPlayerLocation = CachedPlayerLocations.Find(AssignedPlayerIndex);
						if (AssignedPlayerLocation)
						{
							DistanceToAssignedPlayer = FVector::Dist(EnemyLocation, *AssignedPlayerLocation);
							bHasAssignedPlayer = true;
						}
					}
				}

				// Fallback to camera distance for culling purposes
				const FVector LocationDiff = EnemyLocation - CachedCameraLocation;
				const float	  DistanceSq = LocationDiff.SizeSquared();
				const float   DistanceToCamera = FMath::Sqrt(DistanceSq);

				// Cull beyond max distance (from any player's perspective)
				if (DistanceSq > MaxRenderDistanceSq)
				{
					if (VisFragment.RenderMode != EEnemyRenderMode::Hidden)
					{
						if (VisFragment.SkeletalMeshPoolIndex >= 0)
						{
							ReleaseSkeletalMesh(VisFragment.SkeletalMeshPoolIndex);
							VisFragment.SkeletalMeshPoolIndex = INDEX_NONE;
						}
						if (VisFragment.ISMInstanceIndex >= 0)
						{
							ReleaseVATInstance(VisFragment.ISMInstanceIndex, VisFragment.bISMIsWalking);
							VisFragment.ISMInstanceIndex = INDEX_NONE;
						}
						VisFragment.RenderMode = EEnemyRenderMode::Hidden;
						VisFragment.bIsVisible = false;
					}
					continue;
				}

				VisFragment.CachedDistanceToCamera = DistanceToCamera;
				VisFragment.PoolLockTimer = FMath::Max(0.0f, VisFragment.PoolLockTimer - DeltaTime);
				VisFragment.AnimationTime += DeltaTime * VisFragment.AnimationPlayRate;

				// Collect for sorting
				FSkeletalMeshCandidate Entry;
				Entry.Entity = Entities[i];
				Entry.EntityIndex = i;
				Entry.Distance = DistanceToCamera;
				Entry.DistanceToAssignedPlayer = DistanceToAssignedPlayer;
				Entry.VisFragment = &VisFragment;
				Entry.Transform = &Transform;
				Entry.Movement = &Movement;
				Entry.Attack = &Attack;
				Entry.State = &State;
				Entry.Target = &Target;
				CachedAllEntities.Add(Entry);

				// Update skeletal mesh if already assigned
				if (VisFragment.RenderMode == EEnemyRenderMode::SkeletalMesh && VisFragment.SkeletalMeshPoolIndex >= 0)
				{
					UpdateSkeletalMesh(VisFragment.SkeletalMeshPoolIndex, Transform, Movement, Attack, State);
					Attack.bHitPending = false;
				}
			}
		});

	// Sort by distance
	CachedAllEntities.Sort();

	// ========== PASS 2: Assign render modes ==========
	// Count how many enemies should have skeletal meshes based on distance to their ASSIGNED player
	// This ensures enemies only get skeletal mesh when close to the player they're following
	int32 NumShouldHaveSkeletal = 0;
	for (const FSkeletalMeshCandidate& Entry : CachedAllEntities)
	{
		if (NumShouldHaveSkeletal >= SkeletalMeshPoolSize)
			break;
		// Use distance to assigned player - enemies not assigned to any player will have FLT_MAX distance
		if (Entry.DistanceToAssignedPlayer > SkeletalMeshMaxDistance)
			break;
		NumShouldHaveSkeletal++;
	}

	// PHASE 1: Transition far entities from skeletal mesh to ISM (acquire-before-release to prevent flickering)
	for (int32 i = NumShouldHaveSkeletal; i < CachedAllEntities.Num(); ++i)
	{
		FEnemyVisualizationFragment& Vis = *CachedAllEntities[i].VisFragment;
		if (Vis.RenderMode == EEnemyRenderMode::SkeletalMesh && Vis.SkeletalMeshPoolIndex >= 0)
		{
			const FTransform& Transform = *CachedAllEntities[i].Transform;
			const FEnemyMovementFragment& Movement = *CachedAllEntities[i].Movement;

			// Determine walking state for ISM
			const float Speed = Movement.Velocity.Size();
			const bool bIsWalking = Speed > ISMVelocityThreshold;

			// ACQUIRE ISM FIRST before releasing skeletal mesh to prevent flickering
			int32 NewISMIndex = AcquireVATInstance(Transform, Vis, bIsWalking);

			// Now release skeletal mesh
			ReleaseSkeletalMesh(Vis.SkeletalMeshPoolIndex);
			Vis.SkeletalMeshPoolIndex = INDEX_NONE;
			Vis.RenderMode = EEnemyRenderMode::ISM_VAT;
			Vis.ISMInstanceIndex = NewISMIndex;
			Vis.bISMIsWalking = bIsWalking;
		}
	}

	// PHASE 2: Assign skeletal meshes to close entities (with animation sync for smooth transitions)
	// NOTE: Enemies get skeletal mesh if they are close to their ASSIGNED player (have a slot with that player)
	// Even idle enemies get skeletal mesh - the key is they must be assigned to follow that player
	for (int32 i = 0; i < NumShouldHaveSkeletal; ++i)
	{
		FEnemyVisualizationFragment& Vis = *CachedAllEntities[i].VisFragment;
		const FEnemyMovementFragment& Movement = *CachedAllEntities[i].Movement;

		// Already has skeletal mesh - keep it (enemy is close to their assigned player)
		if (Vis.RenderMode == EEnemyRenderMode::SkeletalMesh && Vis.SkeletalMeshPoolIndex >= 0)
		{
			Vis.bPendingSkeletalMeshTransition = false;
			continue;
		}

		// Entity is in ISM mode and should transition to skeletal mesh
		if (Vis.RenderMode == EEnemyRenderMode::ISM_VAT && Vis.ISMInstanceIndex >= 0)
		{
			// Mark as pending transition if not already
			if (!Vis.bPendingSkeletalMeshTransition)
			{
				Vis.bPendingSkeletalMeshTransition = true;
				Vis.PoolLockTimer = 0.0f; // Reset wait timer
			}

			// Update wait timer and animation progress
			Vis.PoolLockTimer += DeltaTime;
			UpdateAnimationCycleProgress(Vis, DeltaTime);

			// Check if we should do the transition now:
			// 1. Animation is at a sync point (near start/end of cycle), OR
			// 2. We've waited too long (force transition)
			const bool bAtSyncPoint = IsAtAnimationSyncPoint(Vis);
			const bool bForceTransition = Vis.PoolLockTimer >= MaxSyncWaitTime;

			if (!bAtSyncPoint && !bForceTransition)
			{
				// Keep waiting for a better moment - but still need to update ISM transform
				// (will be collected in the pending entities batch below)
				continue;
			}
		}

		// Ready to transition - try to acquire skeletal mesh
		if (FreeSkeletalMeshIndices.Num() > 0)
		{
			int32 PoolIndex = AcquireSkeletalMesh(CachedAllEntities[i].Entity, *CachedAllEntities[i].Transform);
			if (PoolIndex >= 0)
			{
				// Successfully acquired skeletal mesh, now release ISM if had one
				if (Vis.ISMInstanceIndex >= 0)
				{
					ReleaseVATInstance(Vis.ISMInstanceIndex, Vis.bISMIsWalking);
					Vis.ISMInstanceIndex = INDEX_NONE;
				}

				Vis.SkeletalMeshPoolIndex = PoolIndex;
				Vis.RenderMode = EEnemyRenderMode::SkeletalMesh;
				Vis.bIsVisible = true;
				Vis.bPendingSkeletalMeshTransition = false;
			}
			// If acquisition failed, keep ISM visible (don't flicker to nothing)
		}
	}

	// ========== PASS 3: Collect ISM data AFTER all assignments ==========
	TArray<FTransform> VATTransforms_Idle;
	TArray<int32>	   VATIndices_Idle;
	TArray<FTransform> VATTransforms_Walk;
	TArray<int32>	   VATIndices_Walk;
	VATTransforms_Idle.Reserve(512);
	VATIndices_Idle.Reserve(512);
	VATTransforms_Walk.Reserve(512);
	VATIndices_Walk.Reserve(512);

	// First, collect pending entities (close to player but waiting for animation sync)
	for (int32 i = 0; i < NumShouldHaveSkeletal; ++i)
	{
		FEnemyVisualizationFragment& Vis = *CachedAllEntities[i].VisFragment;
		if (Vis.bPendingSkeletalMeshTransition && Vis.ISMInstanceIndex >= 0)
		{
			const FTransform& Transform = *CachedAllEntities[i].Transform;
			if (Vis.bISMIsWalking)
			{
				VATTransforms_Walk.Add(Transform);
				VATIndices_Walk.Add(Vis.ISMInstanceIndex);
			}
			else
			{
				VATTransforms_Idle.Add(Transform);
				VATIndices_Idle.Add(Vis.ISMInstanceIndex);
			}
		}
	}

	// Then collect distant ISM entities

	for (int32 i = NumShouldHaveSkeletal; i < CachedAllEntities.Num(); ++i)
	{
		FEnemyVisualizationFragment&  Vis = *CachedAllEntities[i].VisFragment;
		const FTransform&			  Transform = *CachedAllEntities[i].Transform;
		const FEnemyMovementFragment& Movement = *CachedAllEntities[i].Movement;

		// Determine walking state WITH HYSTERESIS to prevent flickering
		const float Speed = Movement.Velocity.Size();
		bool bIsWalking;
		if (Vis.bISMIsWalking)
		{
			// Currently walking - need to drop below threshold minus hysteresis to go idle
			bIsWalking = Speed > (ISMVelocityThreshold - ISMVelocityHysteresis);
		}
		else
		{
			// Currently idle - need to exceed threshold plus hysteresis to start walking
			bIsWalking = Speed > (ISMVelocityThreshold + ISMVelocityHysteresis);
		}

		// Acquire or switch ISM instance
		if (Vis.ISMInstanceIndex < 0)
		{
			Vis.ISMInstanceIndex = AcquireVATInstance(Transform, Vis, bIsWalking);
			Vis.bISMIsWalking = bIsWalking;
		}
		else if (Vis.bISMIsWalking != bIsWalking)
		{
			SwitchISMAnimationState(Vis, Transform, bIsWalking);
		}

		Vis.RenderMode = EEnemyRenderMode::ISM_VAT;
		Vis.bIsVisible = true;

		// Update animation cycle progress for sync point detection
		UpdateAnimationCycleProgress(Vis, DeltaTime);

		// Collect for batch update
		if (Vis.ISMInstanceIndex >= 0)
		{
			if (Vis.bISMIsWalking)
			{
				VATTransforms_Walk.Add(Transform);
				VATIndices_Walk.Add(Vis.ISMInstanceIndex);
			}
			else
			{
				VATTransforms_Idle.Add(Transform);
				VATIndices_Idle.Add(Vis.ISMInstanceIndex);
			}
		}
	}

	static int32 DebugCounter = 0;
	if (++DebugCounter % 60 == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== ISM EXECUTE DEBUG ==="));
		UE_LOG(LogTemp, Warning, TEXT("Total Alive: %d, CachedEntities: %d"), TotalAlive, CachedAllEntities.Num());
		UE_LOG(LogTemp, Warning, TEXT("NumShouldHaveSkeletal: %d"), NumShouldHaveSkeletal);
		UE_LOG(LogTemp, Warning, TEXT("ISM Idle - Transforms: %d, VATISM valid: %d, InstanceCount: %d"),
			VATTransforms_Idle.Num(),
			VATISM != nullptr,
			VATISM ? VATISM->GetInstanceCount() : -1);
		UE_LOG(LogTemp, Warning, TEXT("ISM Walk - Transforms: %d, VATISM_Walk valid: %d, InstanceCount: %d"),
			VATTransforms_Walk.Num(),
			VATISM_Walk != nullptr,
			VATISM_Walk ? VATISM_Walk->GetInstanceCount() : -1);

		if (VATISM && VATTransforms_Idle.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("First Idle Transform: %s"), *VATTransforms_Idle[0].GetLocation().ToString());
			UE_LOG(LogTemp, Warning, TEXT("First Idle Index: %d"), VATIndices_Idle[0]);
		}

		UE_LOG(LogTemp, Warning, TEXT("Camera Location: %s"), *CachedCameraLocation.ToString());
	}

	// ========== PASS 4: Batch update ISM instances ==========
	if (VATTransforms_Idle.Num() > 0 && VATISM)
	{
		BatchUpdateVATInstances(VATTransforms_Idle, VATIndices_Idle, false);
	}
	if (VATTransforms_Walk.Num() > 0 && VATISM_Walk)
	{
		BatchUpdateVATInstances(VATTransforms_Walk, VATIndices_Walk, true);
	}

	// Force render state update even if there were no changes
	if (VATISM)
	{
		VATISM->MarkRenderStateDirty();
	}
	if (VATISM_Walk)
	{
		VATISM_Walk->MarkRenderStateDirty();
	}
}

// ============================================================================
// ANIMATION STATE HELPER
// ============================================================================

EEnemyAnimationState UEnemyVisualizationProcessor::DetermineAnimationState(
	const FEnemyMovementFragment& Movement,
	const FEnemyAttackFragment&	  Attack,
	const FEnemyStateFragment&	  State) const
{
	if (!State.bIsAlive)
	{
		return EEnemyAnimationState::Death;
	}

	if (Attack.bHitPending)
	{
		return EEnemyAnimationState::Hit;
	}

	if (Attack.bIsAttacking)
	{
		return EEnemyAnimationState::Attack;
	}

	if (Movement.Velocity.SizeSquared() > 100.0f)
	{
		return EEnemyAnimationState::Locomotion;
	}

	return EEnemyAnimationState::Idle;
}

bool UEnemyVisualizationProcessor::IsAtAnimationSyncPoint(const FEnemyVisualizationFragment& VisFragment) const
{
	if (!bEnableAnimationSync)
	{
		return true; // Always allow transition if sync is disabled
	}

	// Check if at start or end of animation cycle (within tolerance)
	const float Progress = VisFragment.AnimationCycleProgress;

	// Near cycle start (0.0) or end (1.0)
	const bool bNearStart = Progress < AnimationSyncTolerance;
	const bool bNearEnd = Progress > (1.0f - AnimationSyncTolerance);

	return bNearStart || bNearEnd;
}

void UEnemyVisualizationProcessor::UpdateAnimationCycleProgress(FEnemyVisualizationFragment& VisFragment, float DeltaTime) const
{
	// Get the appropriate cycle duration based on walking state
	const float CycleDuration = VisFragment.bISMIsWalking ? WalkAnimationCycleDuration : IdleAnimationCycleDuration;

	if (CycleDuration <= 0.0f)
	{
		VisFragment.AnimationCycleProgress = 0.0f;
		return;
	}

	// Increment animation time
	VisFragment.AnimationTime += DeltaTime * VisFragment.AnimationPlayRate;

	// Calculate progress through current cycle (0.0 to 1.0)
	VisFragment.AnimationCycleProgress = FMath::Fmod(VisFragment.AnimationTime, CycleDuration) / CycleDuration;
}

// ============================================================================
// RENDER MODE
// ============================================================================

EEnemyRenderMode UEnemyVisualizationProcessor::DetermineRenderMode(float Distance, EEnemyRenderMode CurrentMode) const
{
	float SkeletalThreshold = SkeletalMeshMaxDistance;

	// Hysteresis
	if (CurrentMode == EEnemyRenderMode::SkeletalMesh)
	{
		SkeletalThreshold += LODHysteresis;
	}
	else if (CurrentMode == EEnemyRenderMode::ISM_VAT)
	{
		SkeletalThreshold -= LODHysteresis;
	}

	if (Distance <= SkeletalThreshold)
	{
		return EEnemyRenderMode::SkeletalMesh;
	}

	// Beyond skeletal mesh range - show as cube
	return EEnemyRenderMode::ISM_VAT;
}

void UEnemyVisualizationProcessor::TransitionRenderMode(FMassEntityHandle Entity,
	FEnemyVisualizationFragment& VisFragment, EEnemyRenderMode NewMode, const FTransform& Transform)
{
	// Store old state for acquire-before-release pattern
	int32 OldSkeletalIndex = VisFragment.SkeletalMeshPoolIndex;
	int32 OldISMIndex = VisFragment.ISMInstanceIndex;
	bool bOldISMWasWalking = VisFragment.bISMIsWalking;
	EEnemyRenderMode OldMode = VisFragment.RenderMode;

	// Acquire new representation FIRST
	bool bAcquiredNew = false;
	switch (NewMode)
	{
		case EEnemyRenderMode::SkeletalMesh:
		{
			int32 PoolIndex = AcquireSkeletalMesh(Entity, Transform);
			if (PoolIndex >= 0)
			{
				VisFragment.SkeletalMeshPoolIndex = PoolIndex;
				VisFragment.RenderMode = EEnemyRenderMode::SkeletalMesh;
				bAcquiredNew = true;
			}
			else
			{
				// Fallback to ISM - never hide (default to idle)
				VisFragment.bISMIsWalking = false;
				int32 ISMIndex = AcquireVATInstance(Transform, VisFragment, false);
				if (ISMIndex >= 0)
				{
					VisFragment.ISMInstanceIndex = ISMIndex;
					VisFragment.RenderMode = EEnemyRenderMode::ISM_VAT;
					bAcquiredNew = true;
				}
			}
			break;
		}

		case EEnemyRenderMode::ISM_VAT:
		{
			// Default to idle, will switch if needed
			VisFragment.bISMIsWalking = false;
			int32 InstanceIndex = AcquireVATInstance(Transform, VisFragment, false);
			if (InstanceIndex >= 0)
			{
				VisFragment.ISMInstanceIndex = InstanceIndex;
				VisFragment.RenderMode = EEnemyRenderMode::ISM_VAT;
				bAcquiredNew = true;
			}
			break;
		}

		default:
			// Fallback to ISM (default to idle)
			VisFragment.bISMIsWalking = false;
			int32 ISMIndex = AcquireVATInstance(Transform, VisFragment, false);
			if (ISMIndex >= 0)
			{
				VisFragment.ISMInstanceIndex = ISMIndex;
				VisFragment.RenderMode = EEnemyRenderMode::ISM_VAT;
				bAcquiredNew = true;
			}
			break;
	}

	// Only release old representation AFTER acquiring new one (prevents flickering)
	if (bAcquiredNew)
	{
		// Release old skeletal mesh if we had one and it's different from what we acquired
		if (OldMode == EEnemyRenderMode::SkeletalMesh && OldSkeletalIndex >= 0 &&
			OldSkeletalIndex != VisFragment.SkeletalMeshPoolIndex)
		{
			ReleaseSkeletalMesh(OldSkeletalIndex);
		}

		// Release old ISM if we had one and it's different from what we acquired
		if (OldMode == EEnemyRenderMode::ISM_VAT && OldISMIndex >= 0 &&
			OldISMIndex != VisFragment.ISMInstanceIndex)
		{
			ReleaseVATInstance(OldISMIndex, bOldISMWasWalking);
		}
	}
	// If acquisition failed, keep the old representation visible

	VisFragment.bIsVisible = (VisFragment.RenderMode != EEnemyRenderMode::Hidden);
}

// ============================================================================
// SKELETAL MESH MANAGEMENT
// ============================================================================

int32 UEnemyVisualizationProcessor::AcquireSkeletalMesh(FMassEntityHandle Entity, const FTransform& Transform)
{
	if (FreeSkeletalMeshIndices.Num() == 0)
	{
		return INDEX_NONE;
	}

	int32					PoolIndex = FreeSkeletalMeshIndices.Pop();
	FSkeletalMeshPoolEntry& Entry = SkeletalMeshPool[PoolIndex];

	if (!Entry.IsValid())
	{
		return INDEX_NONE;
	}

	Entry.AssignedEntity = Entity;
	Entry.bInUse = true;

	if (AActor* Actor = Entry.Actor.Get())
	{
		// FIX: Subtract capsule half-height since movement processor adds it
		FTransform AdjustedTransform = Transform;
		FVector	   AdjustedLocation = Transform.GetLocation();
		AdjustedLocation.Z -= 88.0f; // Remove capsule offset to place mesh at ground level
		AdjustedTransform.SetLocation(AdjustedLocation);
		AdjustedTransform.SetScale3D(FVector(0.4f, 0.4f, 0.4f)); // Scale to 40% size

		Actor->SetActorTransform(AdjustedTransform);
		Actor->SetActorHiddenInGame(false);
	}

	if (USkeletalMeshComponent* SkelMesh = Entry.SkeletalMeshComponent.Get())
	{
		SkelMesh->SetComponentTickEnabled(true);
		// Force animation to start updating immediately
		SkelMesh->bRecentlyRendered = true;
		SkelMesh->SetForcedLOD(0); // Force highest LOD for nearby enemies
	}

	// Reset animation instance
	if (UEnemyAnimInstance* AnimInst = Entry.AnimInstance.Get())
	{
		AnimInst->ResetToIdle();
		// Animation will update automatically once component ticking is enabled
	}

	return PoolIndex;
}

void UEnemyVisualizationProcessor::ReleaseSkeletalMesh(int32 PoolIndex)
{
	if (!SkeletalMeshPool.IsValidIndex(PoolIndex))
	{
		return;
	}

	FSkeletalMeshPoolEntry& Entry = SkeletalMeshPool[PoolIndex];

	if (AActor* Actor = Entry.Actor.Get())
	{
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorLocation(FVector(0, 0, -10000));
	}

	if (USkeletalMeshComponent* SkelMesh = Entry.SkeletalMeshComponent.Get())
	{
		SkelMesh->SetComponentTickEnabled(false);
	}

	if (UEnemyAnimInstance* AnimInst = Entry.AnimInstance.Get())
	{
		AnimInst->ResetToIdle();
	}

	Entry.AssignedEntity = FMassEntityHandle();
	Entry.bInUse = false;

	FreeSkeletalMeshIndices.Add(PoolIndex);
}

void UEnemyVisualizationProcessor::UpdateSkeletalMesh(
	int32						  PoolIndex,
	const FTransform&			  Transform,
	const FEnemyMovementFragment& Movement,
	const FEnemyAttackFragment&	  Attack,
	const FEnemyStateFragment&	  State)
{
	if (!SkeletalMeshPool.IsValidIndex(PoolIndex))
	{
		return;
	}

	FSkeletalMeshPoolEntry& Entry = SkeletalMeshPool[PoolIndex];

	// Update transform
	if (AActor* Actor = Entry.Actor.Get())
	{
		// FIX: Subtract capsule half-height since movement processor adds it
		FTransform AdjustedTransform = Transform;
		FVector	   AdjustedLocation = Transform.GetLocation();
		AdjustedLocation.Z -= 88.0f; // Remove capsule offset to place mesh at ground level
		AdjustedTransform.SetLocation(AdjustedLocation);

		FRotator AdjustedRotation = AdjustedTransform.Rotator();
		AdjustedRotation.Yaw -= 90.0f; // Compensate for mesh orientation
		AdjustedTransform.SetRotation(AdjustedRotation.Quaternion());

		AdjustedTransform.SetScale3D(FVector(0.4f, 0.4f, 0.4f)); // Scale to 40% size

		Actor->SetActorTransform(AdjustedTransform);

		// Debug: Log first few updates
		static int32 UpdateLogCount = 0;
		if (UpdateLogCount < 5)
		{
			UE_LOG(LogTemp, Warning, TEXT("UpdateSkeletalMesh[%d]: Entity pos %s -> Mesh pos %s, Velocity=%s"),
				PoolIndex, *Transform.GetLocation().ToString(), *AdjustedLocation.ToString(), *Movement.Velocity.ToString());
			UpdateLogCount++;
		}
	}

	// Update animation instance with all the data
	if (UEnemyAnimInstance* AnimInst = Entry.AnimInstance.Get())
	{
		// Calculate acceleration from velocity if not set
		FVector AccelerationToUse = Movement.Acceleration;
		if (AccelerationToUse.IsNearlyZero() && !Movement.Velocity.IsNearlyZero())
		{
			// Derive acceleration from velocity (for animation purposes)
			AccelerationToUse = Movement.Velocity.GetSafeNormal() * Movement.MaxAcceleration;
		}

		// Update movement - this sets Direction, GroundSpeed, etc.
		AnimInst->UpdateMovement(
			Movement.Velocity,
			AccelerationToUse,
			Movement.MaxSpeed,
			Movement.bIsFalling,
			Movement.FacingDirection);

		// Handle combat state
		if (!State.bIsAlive)
		{
			AnimInst->TriggerDeath();
		}
		else if (Attack.bHitPending)
		{
			AnimInst->TriggerHitReaction(Attack.HitDirection);
		}
		else if (Attack.bIsAttacking)
		{
			AnimInst->SetCombatState(EEnemyAnimationState::Attack, true, Attack.AttackType);
		}
		else
		{
			// Let locomotion/idle be determined by movement
			EEnemyAnimationState NewState = (Movement.Velocity.SizeSquared() > 100.0f)
				? EEnemyAnimationState::Locomotion
				: EEnemyAnimationState::Idle;
			AnimInst->SetCombatState(NewState, false, 0);
		}

		// Update look at target if available
		if (Attack.bHasLookAtTarget)
		{
			FVector	 ToTarget = Attack.LookAtTarget - Transform.GetLocation();
			FRotator LookRot = ToTarget.Rotation();
			AnimInst->SetLookRotation(LookRot);
		}
	}
}

// ============================================================================
// VAT/ISM MANAGEMENT
// ============================================================================

int32 UEnemyVisualizationProcessor::AcquireVATInstance(const FTransform& Transform, const FEnemyVisualizationFragment& VisFragment, bool bIsWalking)
{
	// Select the appropriate ISM component based on walking state
	UInstancedStaticMeshComponent* TargetISM = bIsWalking ? VATISM_Walk : VATISM;
	TArray<int32>&							   FreeIndices = bIsWalking ? FreeVATInstanceIndices_Walk : FreeVATInstanceIndices;

	if (!TargetISM || !TargetISM->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("AcquireVATInstance: TargetISM is NULL! bIsWalking=%d"), bIsWalking);
		// Try to initialize if not ready
		UWorld* World = GetWorld();
		if (World)
		{
			InitializeVATSystem(World);
		}

		TargetISM = bIsWalking ? VATISM_Walk : VATISM;
		if (!TargetISM)
			return INDEX_NONE;
	}

	int32 InstanceIndex = INDEX_NONE;

	// Adjust transform for static mesh mannequin
	FTransform CubeTransform = Transform;
	FVector	   Location = Transform.GetLocation();
	Location.Z -= 88.0f; // Match skeletal mesh offset
	CubeTransform.SetLocation(Location);

	// Rotate to match skeletal mesh orientation
	FRotator Rotation = CubeTransform.Rotator();
	Rotation.Yaw -= 90.0f;
	CubeTransform.SetRotation(Rotation.Quaternion());

	CubeTransform.SetScale3D(FVector(0.4f, 0.4f, 0.4f)); // Scale to 40% size

	if (FreeIndices.Num() > 0)
	{
		InstanceIndex = FreeIndices.Pop();
		TargetISM->UpdateInstanceTransform(InstanceIndex, CubeTransform, false, false, false);
	}
	else
	{
		InstanceIndex = TargetISM->AddInstance(CubeTransform, false);
	}

	// DEBUG
	static int32 AcquireLogCount = 0;
	if (AcquireLogCount++ < 10)
	{
		UE_LOG(LogTemp, Warning, TEXT("AcquireVATInstance: Index=%d, Location=%s, Walking=%d, TotalInstances=%d"),
			InstanceIndex, *CubeTransform.GetLocation().ToString(), bIsWalking, TargetISM->GetInstanceCount());
	}

	return InstanceIndex;
}

void UEnemyVisualizationProcessor::ReleaseVATInstance(int32 InstanceIndex, bool bIsWalking)
{
	// Select the appropriate ISM component based on walking state
	UInstancedStaticMeshComponent* TargetISM = bIsWalking ? VATISM_Walk : VATISM;
	TArray<int32>&							   FreeIndices = bIsWalking ? FreeVATInstanceIndices_Walk : FreeVATInstanceIndices;

	if (InstanceIndex < 0 || !TargetISM || !TargetISM->IsValidLowLevel())
	{
		return;
	}

	FreeIndices.Add(InstanceIndex);

	FTransform HiddenTransform;
	HiddenTransform.SetLocation(FVector(0, 0, -100000.0f));
	TargetISM->UpdateInstanceTransform(InstanceIndex, HiddenTransform, false, false, false);
}

void UEnemyVisualizationProcessor::SwitchISMAnimationState(FEnemyVisualizationFragment& VisFragment, const FTransform& Transform, bool bNewIsWalking)
{
	// Check if already in the correct state
	bool bCurrentlyWalking = VisFragment.bISMIsWalking;
	if (bCurrentlyWalking == bNewIsWalking)
	{
		return; // No change needed
	}

	// ACQUIRE-BEFORE-RELEASE: Get new instance first to prevent flickering
	int32 OldInstanceIndex = VisFragment.ISMInstanceIndex;
	bool bOldWasWalking = bCurrentlyWalking;

	// Acquire in new ISM first
	int32 NewInstanceIndex = AcquireVATInstance(Transform, VisFragment, bNewIsWalking);

	if (NewInstanceIndex >= 0)
	{
		// Successfully acquired new instance, now release old one
		VisFragment.ISMInstanceIndex = NewInstanceIndex;
		VisFragment.bISMIsWalking = bNewIsWalking;

		if (OldInstanceIndex >= 0)
		{
			ReleaseVATInstance(OldInstanceIndex, bOldWasWalking);
		}
	}
	// If acquisition failed, keep the old instance (don't flicker to nothing)
}

void UEnemyVisualizationProcessor::BatchUpdateVATInstances(
	const TArray<FTransform>& Transforms,
	const TArray<int32>&	  Indices,
	bool					  bIsWalking)
{
	UInstancedStaticMeshComponent* TargetISM = bIsWalking ? VATISM_Walk : VATISM;

	if (!TargetISM || !TargetISM->IsValidLowLevel() || Transforms.Num() == 0)
	{
		return;
	}

	const int32 MaxValidIndex = TargetISM->GetInstanceCount();

	for (int32 i = 0; i < Transforms.Num(); ++i)
	{
		const int32 InstanceIndex = Indices[i];

		if (InstanceIndex < 0 || InstanceIndex >= MaxValidIndex)
		{
			continue;
		}

		FTransform AdjustedTransform = Transforms[i];
		FVector	   Location = AdjustedTransform.GetLocation();
		Location.Z -= 88.0f;
		AdjustedTransform.SetLocation(Location);

		FRotator Rotation = AdjustedTransform.Rotator();
		Rotation.Yaw -= 90.0f;
		AdjustedTransform.SetRotation(Rotation.Quaternion());

		AdjustedTransform.SetScale3D(FVector(0.4f));

		// Последний параметр TRUE - телепортация (обновляет physics/bounds)
		TargetISM->UpdateInstanceTransform(InstanceIndex, AdjustedTransform, true, true, true);
	}

	// КРИТИЧНО: Эти вызовы заставляют HISM перестроить дерево и обновить рендер
	TargetISM->MarkRenderStateDirty();
	TargetISM->MarkRenderTransformDirty();

	// Принудительное обновление bounds - это часто решает проблему "невидимости"
	TargetISM->UpdateBounds();

	// Для HISM - перестроить BVH дерево
	// TargetISM->BuildTreeIfOutdated(true, false);
}

// ============================================================================
// VAT HELPERS
// ============================================================================

FVector4 UEnemyVisualizationProcessor::CalculateVATCustomData(EEnemyAnimationState AnimState, float AnimTime) const
{
	const FVATAnimationData* AnimData = GetVATAnimationData(AnimState);

	if (!AnimData)
	{
		return FVector4(0.0f, 30.0f, FMath::Fmod(AnimTime, 1.0f), 1.0f);
	}

	float NormalizedTime;
	if (AnimData->bLooping)
	{
		NormalizedTime = FMath::Fmod(AnimTime, AnimData->Duration) / AnimData->Duration;
	}
	else
	{
		NormalizedTime = FMath::Clamp(AnimTime / AnimData->Duration, 0.0f, 1.0f);
	}

	const float TotalFrames = FMath::Max(1, VATConfig.TotalFrames);

	return FVector4(
		AnimData->StartFrame / TotalFrames,
		AnimData->EndFrame / TotalFrames,
		NormalizedTime,
		1.0f);
}

const FVATAnimationData* UEnemyVisualizationProcessor::GetVATAnimationData(EEnemyAnimationState AnimState) const
{
	for (const FVATAnimationData& Data : VATConfig.Animations)
	{
		if (Data.AnimationType == AnimState)
		{
			return &Data;
		}
	}
	return nullptr;
}

// ============================================================================
// CLEANUP
// ============================================================================

void UEnemyVisualizationProcessor::BeginDestroy()
{
	// Cleanup skeletal mesh pool actors
	for (FSkeletalMeshPoolEntry& Entry : SkeletalMeshPool)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			Actor->Destroy();
		}
	}
	SkeletalMeshPool.Empty();
	FreeSkeletalMeshIndices.Empty();

	// Cleanup VAT visualization actor (holds both idle and walk ISM components)
	if (VATVisualizationActor)
	{
		VATVisualizationActor->Destroy();
		VATVisualizationActor = nullptr;
	}
	VATISM = nullptr;
	VATISM_Walk = nullptr;
	FreeVATInstanceIndices.Empty();
	FreeVATInstanceIndices_Walk.Empty();

	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("EnemyVisualizationProcessor: Cleaned up visualization actors"));

	Super::BeginDestroy();
}

// ============================================================================
// PUBLIC API
// ============================================================================

void UEnemyVisualizationProcessor::SetVATRenderingEnabled(bool bEnabled)
{
	bEnableVATRendering = bEnabled;
}

void UEnemyVisualizationProcessor::SetLODDistances(float SkeletalMaxDist, float VATMaxDist)
{
	SkeletalMeshMaxDistance = SkeletalMaxDist;
	VATMaxDistance = VATMaxDist;
}

void UEnemyVisualizationProcessor::GetVisualizationStats(int32& OutSkeletalMeshCount, int32& OutVATInstanceCount) const
{
	OutSkeletalMeshCount = SkeletalMeshPoolSize - FreeSkeletalMeshIndices.Num();

	if (VATISM)
	{
		OutVATInstanceCount = VATISM->GetInstanceCount() - FreeVATInstanceIndices.Num();
	}
	else
	{
		OutVATInstanceCount = 0;
	}
}

// ============================================================================
// CLEANUP OBSERVER
// ============================================================================

UEnemyVisualizationCleanupObserver::UEnemyVisualizationCleanupObserver()
	: EntityQuery(*this)
{
	ObservedType = FEnemyVisualizationFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
}

void UEnemyVisualizationCleanupObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FEnemyVisualizationFragment>(EMassFragmentAccess::ReadOnly);
}

void UEnemyVisualizationCleanupObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTemp, Verbose, TEXT("EnemyVisualizationCleanupObserver: Entity removed"));
}
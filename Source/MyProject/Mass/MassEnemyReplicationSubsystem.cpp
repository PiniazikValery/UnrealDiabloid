// Fill out your copyright notice in the Description page of Project Settings.

#include "MassEnemyReplicationSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MyProjectPlayerController.h"
#include "MassEntitySubsystem.h"
#include "MassCommonFragments.h"
#include "EnemyFragments.h"
#include "EnemyVisualizationProcessor.h"
#include "UObject/UObjectIterator.h"

void UMassEnemyReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("MassEnemyReplicationSubsystem: Initialized"));
}

void UMassEnemyReplicationSubsystem::Deinitialize()
{
	Super::Deinitialize();

	PendingClientBatches.Empty();
	DestroyedNetworkIDs.Empty();

	UE_LOG(LogTemp, Log, TEXT("MassEnemyReplicationSubsystem: Deinitialized"));
}

bool UMassEnemyReplicationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create on both server and client
	// Server: manages NetworkIDs, relevancy, priority
	// Client: stores received batches for reception processor
	UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr;
}

int32 UMassEnemyReplicationSubsystem::AssignNetworkID()
{
	// Always use incrementing IDs - never reuse
	// This prevents ghost entity bugs where stale network batches
	// get confused with new entities using recycled IDs
	// With 32-bit int, we have 2+ billion IDs - plenty for any session
	return NextNetworkID++;
}

void UMassEnemyReplicationSubsystem::ReleaseNetworkID(int32 NetworkID)
{
	// No-op: We no longer reuse NetworkIDs to prevent ghost entity bugs
	// Keeping the function for API compatibility
}

bool UMassEnemyReplicationSubsystem::IsEntityRelevant(const FVector& EntityLocation, TArray<APlayerController*>& OutRelevantPlayers) const
{
	OutRelevantPlayers.Empty();

	TArray<APlayerController*> AllPlayers = GetAllPlayerControllers();

	for (APlayerController* PC : AllPlayers)
	{
		if (!PC || !PC->GetPawn())
			continue;

		// Use player pawn location for relevancy check
		FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
		float DistanceSquared = FVector::DistSquared(EntityLocation, PlayerLocation);

		if (DistanceSquared <= (RelevancyRadius * RelevancyRadius))
		{
			OutRelevantPlayers.Add(PC);
		}
	}

	return OutRelevantPlayers.Num() > 0;
}

uint8 UMassEnemyReplicationSubsystem::CalculateReplicationPriority(const FVector& EntityLocation, const FVector& PlayerLocation) const
{
	float Distance = FVector::Dist(EntityLocation, PlayerLocation);

	// Priority: 255 at distance 0, 0 at relevancy radius
	float NormalizedDistance = FMath::Clamp(Distance / RelevancyRadius, 0.0f, 1.0f);
	uint8 Priority = static_cast<uint8>((1.0f - NormalizedDistance) * 255.0f);

	return Priority;
}

float UMassEnemyReplicationSubsystem::GetReplicationInterval(float Distance) const
{
	if (Distance < NearDistance)
	{
		return NearUpdateInterval;  // 20 Hz
	}
	else if (Distance < MidDistance)
	{
		return MidUpdateInterval;   // 10 Hz
	}
	else if (Distance < FarDistance)
	{
		return FarUpdateInterval;   // 5 Hz
	}
	else
	{
		return 999.0f;  // Don't replicate (beyond relevancy radius)
	}
}

TArray<APlayerController*> UMassEnemyReplicationSubsystem::GetAllPlayerControllers() const
{
	TArray<APlayerController*> PlayerControllers;

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				PlayerControllers.Add(PC);
			}
		}
	}

	return PlayerControllers;
}

void UMassEnemyReplicationSubsystem::StoreBatchForClient(APlayerController* Client, const FMassEntityBatchUpdate& Batch)
{
	if (Client)
	{
		PendingClientBatches.Add(Client, Batch);
	}
}

bool UMassEnemyReplicationSubsystem::GetAndClearBatchForClient(APlayerController* Client, FMassEntityBatchUpdate& OutBatch)
{
	if (FMassEntityBatchUpdate* Found = PendingClientBatches.Find(Client))
	{
		OutBatch = *Found;
		PendingClientBatches.Remove(Client);
		return true;
	}
	return false;
}

void UMassEnemyReplicationSubsystem::QueueBatchForSending(APlayerController* Client, const FMassEntityBatchUpdate& Batch)
{
	if (!Client)
	{
		return;
	}

	// Thread-safe add to queue (called from MASS worker thread)
	FScopeLock Lock(&QueuedBatchesLock);
	TArray<FMassEntityBatchUpdate>& Batches = QueuedBatchesToSend.FindOrAdd(Client);
	Batches.Add(Batch);
}

void UMassEnemyReplicationSubsystem::QueueDeathNotification(int32 NetworkID)
{
	if (NetworkID == INDEX_NONE)
	{
		return;
	}

	FScopeLock Lock(&DeathNotificationsLock);
	QueuedDeathNotifications.AddUnique(NetworkID);
	UE_LOG(LogTemp, Error, TEXT("[VIS-DEBUG] !!!!! DEATH QUEUED on SERVER for NetworkID %d !!!!!"), NetworkID);
}

void UMassEnemyReplicationSubsystem::HandleDeathNotifications(const TArray<int32>& NetworkIDs)
{
	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] ===== HandleDeathNotifications ENTERED with %d NetworkIDs ====="), NetworkIDs.Num());

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: World is NULL!"));
		return;
	}

	const ENetMode NetMode = World->GetNetMode();
	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: World=%s, NetMode=%d (0=Standalone, 1=DedicatedServer, 2=ListenServer, 3=Client)"),
		*World->GetName(), (int32)NetMode);

	// Only skip on dedicated server - clients AND listen servers need to handle death
	if (NetMode == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: Skipping on dedicated server"));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: Getting EntitySubsystem..."));
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: No EntitySubsystem!"));
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: EntitySubsystem found, getting EntityManager..."));

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: EntityManager obtained, checking NetworkIDToEntity map size: %d"), NetworkIDToEntity.Num());

	// Try to get VisProcessor for proper cleanup
	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: Getting VisProcessor..."));
	UEnemyVisualizationProcessor* VisProcessor = UEnemyVisualizationProcessor::GetInstanceForWorld(World);
	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: VisProcessor = %s"), VisProcessor ? TEXT("FOUND") : TEXT("NULL"));

	UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] HandleDeathNotifications: Starting to process %d death notifications..."), NetworkIDs.Num());

	for (int32 NetworkID : NetworkIDs)
	{
		UE_LOG(LogTemp, Error, TEXT("[VIS-DEBUG] !!!!! DEATH NOTIFICATION RECEIVED for NetworkID %d !!!!!"), NetworkID);

		FMassEntityHandle* EntityHandlePtr = NetworkIDToEntity.Find(NetworkID);
		if (!EntityHandlePtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] NetworkID %d not found in client entity map (map has %d entries)"), NetworkID, NetworkIDToEntity.Num());
			continue;
		}

		FMassEntityHandle EntityHandle = *EntityHandlePtr;
		if (!EntityManager.IsEntityValid(EntityHandle))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Entity for NetworkID %d is invalid"), NetworkID);
			NetworkIDToEntity.Remove(NetworkID);
			continue;
		}

		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Found valid entity for NetworkID %d, cleaning up..."), NetworkID);

		// Clean up visualization BEFORE destroying entity
		if (VisProcessor)
		{
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Calling CleanupEntityVisualization..."));
			VisProcessor->CleanupEntityVisualization(EntityHandle, EntityManager);
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] CleanupEntityVisualization done for NetworkID %d"), NetworkID);
		}
		else
		{
			// Fallback: Try to manually release visualization resources
			FEnemyVisualizationFragment* VisFragment = EntityManager.GetFragmentDataPtr<FEnemyVisualizationFragment>(EntityHandle);
			if (VisFragment)
			{
				UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Manual cleanup for NetworkID %d - ISMIdx: %d, SkelIdx: %d"),
					NetworkID, VisFragment->ISMInstanceIndex, VisFragment->SkeletalMeshPoolIndex);
				// Mark as hidden so the processor won't render it
				VisFragment->RenderMode = EEnemyRenderMode::Hidden;
				VisFragment->bIsVisible = false;
			}
		}

		// Destroy the client entity
		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] About to destroy entity for NetworkID %d, IsProcessing=%s"),
			NetworkID, EntityManager.IsProcessing() ? TEXT("TRUE") : TEXT("FALSE"));

		if (EntityManager.IsProcessing())
		{
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Using DEFERRED destruction for NetworkID %d"), NetworkID);
			EntityManager.Defer().DestroyEntity(EntityHandle);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Using IMMEDIATE destruction for NetworkID %d"), NetworkID);
			EntityManager.DestroyEntity(EntityHandle);
		}

		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Entity destruction called for NetworkID %d"), NetworkID);

		// Remove from tracking maps and mark as destroyed
		NetworkIDToEntity.Remove(NetworkID);
		NetworkIDLastUpdateTime.Remove(NetworkID);
		DestroyedNetworkIDs.Add(NetworkID);

		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Client destroyed entity for NetworkID %d, remaining tracked: %d"),
			NetworkID, NetworkIDToEntity.Num());
	}
}

TStatId UMassEnemyReplicationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMassEnemyReplicationSubsystem, STATGROUP_Tickables);
}

void UMassEnemyReplicationSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Client-side ONLY: Process received batches and create/update entities
	// Listen servers should NOT create shadow entities - they see the real server entities
	if (World->GetNetMode() == NM_Client)
	{
		ProcessClientReception(DeltaTime);
		return;
	}

	// Server-side (dedicated or listen): Send all queued batches via RPC (this runs on game thread)
	TMap<APlayerController*, TArray<FMassEntityBatchUpdate>> BatchesToSend;
	{
		FScopeLock Lock(&QueuedBatchesLock);
		BatchesToSend = MoveTemp(QueuedBatchesToSend);
		QueuedBatchesToSend.Empty();
	}

	// Get queued death notifications
	TArray<int32> DeathNotificationsToSend;
	{
		FScopeLock Lock(&DeathNotificationsLock);
		DeathNotificationsToSend = MoveTemp(QueuedDeathNotifications);
		QueuedDeathNotifications.Empty();
	}

	// Send death notifications to ALL clients
	if (DeathNotificationsToSend.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Server sending %d death notifications"), DeathNotificationsToSend.Num());

		TArray<APlayerController*> AllPlayers = GetAllPlayerControllers();
		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Found %d player controllers"), AllPlayers.Num());

		int32 SentCount = 0;
		for (APlayerController* PC : AllPlayers)
		{
			if (!PC)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Skipping NULL player controller"));
				continue;
			}

			if (PC->IsLocalController())
			{
				UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Skipping local controller: %s"), *PC->GetName());
				continue;  // Skip local player on listen server
			}

			AMyProjectPlayerController* MyPC = Cast<AMyProjectPlayerController>(PC);
			if (MyPC)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Sending death notifications to client: %s"), *MyPC->GetName());
				MyPC->ClientReceiveDeathNotifications(DeathNotificationsToSend);
				SentCount++;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Cast to MyProjectPlayerController failed for: %s"), *PC->GetName());
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Sent death notifications to %d clients"), SentCount);
	}

	if (BatchesToSend.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION] Subsystem Tick: Sending batches to %d clients"), BatchesToSend.Num());
	}

	// Send RPCs (safe on game thread)
	for (const auto& Pair : BatchesToSend)
	{
		APlayerController* Client = Pair.Key;
		const TArray<FMassEntityBatchUpdate>& Batches = Pair.Value;

		if (!Client)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Subsystem Tick: Null client!"));
			continue;
		}

		AMyProjectPlayerController* MyPC = Cast<AMyProjectPlayerController>(Client);
		if (!MyPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Subsystem Tick: Client %s is not AMyProjectPlayerController!"),
				*Client->GetName());
			continue;
		}

		int32 TotalEntities = 0;
		// Send each batch
		for (const FMassEntityBatchUpdate& Batch : Batches)
		{
			if (Batch.Entities.Num() > 0)
			{
				MyPC->ClientReceiveMassEntityBatch(Batch);
				TotalEntities += Batch.Entities.Num();
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION] Subsystem Tick: Sent %d batches (%d entities) to client %s"),
			Batches.Num(), TotalEntities, *Client->GetName());
	}
}

void UMassEnemyReplicationSubsystem::ProcessClientReception(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	// Get local player controller
	APlayerController* LocalPC = World->GetFirstPlayerController();
	if (!LocalPC)
	{
		return;
	}

	// Try to get batch data
	FMassEntityBatchUpdate BatchData;
	bool bReceivedData = GetAndClearBatchForClient(LocalPC, BatchData);

	if (bReceivedData)
	{
		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION] Client Reception: Processing %d entities"), BatchData.Entities.Num());

		// Get entity manager
		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!EntitySubsystem)
		{
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Client Reception: No EntitySubsystem!"));
			return;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		// Process each entity in the batch
		for (const FCompressedEnemyState& State : BatchData.Entities)
		{
			// Track update time for staleness detection
			NetworkIDLastUpdateTime.Add(State.NetworkID, CurrentTime);

			if (FMassEntityHandle* ExistingEntity = NetworkIDToEntity.Find(State.NetworkID))
			{
				// Update existing entity
				if (EntityManager.IsEntityValid(*ExistingEntity))
				{
					UpdateClientEntity(*ExistingEntity, State);
				}
				else
				{
					// Entity is invalid, remove from map and create new one
					NetworkIDToEntity.Remove(State.NetworkID);
					CreateClientEntity(State);
				}
			}
			else
			{
				// Create new entity
				CreateClientEntity(State);
			}
		}
	}

	// Periodic debug summary - only log on state changes or every 5 seconds
	static int32 LastTrackedCount = -1;
	static int32 LastStaleCount = -1;

	DebugLogTimer += DeltaTime;

	// Count stale entities (no update in last 2 seconds)
	int32 StaleCount = 0;
	int32 ActiveCount = 0;
	const float StaleThreshold = 2.0f;

	for (const auto& Pair : NetworkIDLastUpdateTime)
	{
		float TimeSinceUpdate = CurrentTime - Pair.Value;
		if (TimeSinceUpdate > StaleThreshold)
		{
			StaleCount++;
		}
		else
		{
			ActiveCount++;
		}
	}

	const bool bClientStateChanged = (NetworkIDToEntity.Num() != LastTrackedCount) || (StaleCount != LastStaleCount);

	if (bClientStateChanged || DebugLogTimer >= 5.0f)
	{
		if (bClientStateChanged)
		{
			UE_LOG(LogTemp, Error, TEXT("[VIS-DEBUG] !!!!! CLIENT STATE CHANGE !!!!!"));
			UE_LOG(LogTemp, Error, TEXT("[VIS-DEBUG] Tracked: %d->%d, Stale: %d->%d"),
				LastTrackedCount, NetworkIDToEntity.Num(), LastStaleCount, StaleCount);

			// Log stale entities on state change
			for (const auto& Pair : NetworkIDLastUpdateTime)
			{
				float TimeSinceUpdate = CurrentTime - Pair.Value;
				if (TimeSinceUpdate > StaleThreshold)
				{
					UE_LOG(LogTemp, Warning, TEXT("[VIS-DEBUG] STALE ENTITY: NetworkID=%d, TimeSinceUpdate=%.1fs"),
						Pair.Key, TimeSinceUpdate);
				}
			}
		}

		LastTrackedCount = NetworkIDToEntity.Num();
		LastStaleCount = StaleCount;
		DebugLogTimer = 0.0f;

		UE_LOG(LogTemp, Warning, TEXT("[VIS-DEBUG] === CLIENT ENTITY SUMMARY ==="));
		UE_LOG(LogTemp, Warning, TEXT("[VIS-DEBUG] TotalTracked: %d, Active: %d, Stale(>%.0fs): %d"),
			NetworkIDToEntity.Num(), ActiveCount, StaleThreshold, StaleCount);

		// Log player position for reference
		if (LocalPC && LocalPC->GetPawn())
		{
			UE_LOG(LogTemp, Warning, TEXT("[VIS-DEBUG] LocalPlayerPos: %s"),
				*LocalPC->GetPawn()->GetActorLocation().ToString());
		}
	}
}

void UMassEnemyReplicationSubsystem::CreateClientEntity(const FCompressedEnemyState& State)
{
	// Don't recreate entities that were destroyed - prevents ghost entities from stale batches
	if (DestroyedNetworkIDs.Contains(State.NetworkID))
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Create archetype if needed (cached)
	if (!ClientEntityArchetype.IsValid())
	{
		ClientEntityArchetype = EntityManager.CreateArchetype(
			TArray<const UScriptStruct*>{
				FTransformFragment::StaticStruct(),
				FEnemyTargetFragment::StaticStruct(),
				FEnemyAttackFragment::StaticStruct(),
				FEnemyMovementFragment::StaticStruct(),
				FEnemyStateFragment::StaticStruct(),
				FEnemyVisualizationFragment::StaticStruct(),
				FEnemyNetworkFragment::StaticStruct(),
				FEnemyTag::StaticStruct()
			}
		);
	}

	// Create entity (safe here - outside of MASS processing)
	FMassEntityHandle NewEntity = EntityManager.CreateEntity(ClientEntityArchetype);

	if (!EntityManager.IsEntityValid(NewEntity))
	{
		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Failed to create client entity for NetworkID %d"), State.NetworkID);
		return;
	}

	// Set initial state
	UpdateClientEntity(NewEntity, State);

	// Store mapping
	NetworkIDToEntity.Add(State.NetworkID, NewEntity);

	UE_LOG(LogTemp, Warning, TEXT("[VIS-DEBUG] CLIENT ENTITY CREATED: NetworkID=%d, Pos=%s, TargetPlayerIdx=%d, TotalClientEntities=%d"),
		State.NetworkID, *State.Position.ToString(), State.TargetPlayerIndex, NetworkIDToEntity.Num());
}

void UMassEnemyReplicationSubsystem::UpdateClientEntity(FMassEntityHandle EntityHandle, const FCompressedEnemyState& State)
{
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return;
	}

	// Get Network fragment for interpolation
	FEnemyNetworkFragment& Network = EntityManager.GetFragmentDataChecked<FEnemyNetworkFragment>(EntityHandle);
	FTransformFragment& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(EntityHandle);

	// Decode yaw
	float NewYaw = ((static_cast<float>(State.RotationYaw) / 65535.0f) * 360.0f) - 180.0f;

	if (!Network.bHasReceivedFirstUpdate)
	{
		// First update - snap directly to position
		Transform.SetTransform(FTransform(FRotator(0.0f, NewYaw, 0.0f), State.Position, FVector::OneVector));

		Network.PreviousPosition = State.Position;
		Network.TargetPosition = State.Position;
		Network.PreviousYaw = NewYaw;
		Network.TargetYaw = NewYaw;
		Network.PreviousVelocity = State.Velocity;
		Network.TargetVelocity = State.Velocity;
		Network.InterpolationAlpha = 1.0f;
		Network.bHasReceivedFirstUpdate = true;

		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION-LAG] First update for NetworkID %d at %s"), State.NetworkID, *State.Position.ToString());
	}
	else
	{
		// Subsequent updates - set up interpolation

		// Current interpolated position becomes new "previous"
		FVector CurrentPos = Transform.GetTransform().GetLocation();
		FRotator CurrentRot = Transform.GetTransform().GetRotation().Rotator();

		Network.PreviousPosition = CurrentPos;
		Network.PreviousYaw = CurrentRot.Yaw;
		Network.PreviousVelocity = Network.TargetVelocity;

		// New server data becomes target
		Network.TargetPosition = State.Position;
		Network.TargetYaw = NewYaw;
		Network.TargetVelocity = State.Velocity;

		// Calculate expected interval based on time since last update
		if (Network.TimeSinceLastUpdate > 0.01f)
		{
			// Smooth the expected interval estimate
			Network.ExpectedUpdateInterval = FMath::Lerp(
				Network.ExpectedUpdateInterval,
				Network.TimeSinceLastUpdate,
				0.3f  // Blend factor
			);
		}

		// Reset interpolation
		Network.InterpolationAlpha = 0.0f;
		Network.TimeSinceLastUpdate = 0.0f;
	}

	// Update State fragment
	FEnemyStateFragment& EnemyState = EntityManager.GetFragmentDataChecked<FEnemyStateFragment>(EntityHandle);
	EnemyState.Health = (static_cast<float>(State.Health) / 255.0f) * 100.0f;
	EnemyState.bIsAlive = State.IsAlive();
	EnemyState.bIsMoving = State.IsMoving();

	// Update Movement fragment
	FEnemyMovementFragment& Movement = EntityManager.GetFragmentDataChecked<FEnemyMovementFragment>(EntityHandle);
	Movement.Velocity = State.Velocity;

	// Update Attack fragment
	FEnemyAttackFragment& Attack = EntityManager.GetFragmentDataChecked<FEnemyAttackFragment>(EntityHandle);
	const bool bWasAttacking = Attack.bIsAttacking;
	Attack.bIsAttacking = State.IsAttacking();

	// If attack just started (transition from not attacking to attacking), trigger montage on client
	if (Attack.bIsAttacking && !bWasAttacking)
	{
		Attack.bShouldTriggerAttackMontage = true;
	}

	// Update Network ID and replicated data
	Network.NetworkID = State.NetworkID;
	Network.ReplicatedPosition = State.Position;
	Network.ReplicatedRotationYaw = State.RotationYaw;
	Network.ReplicatedHealth = State.Health;
	Network.ReplicatedFlags = State.Flags;
	Network.ReplicatedVelocity = State.Velocity;
	Network.TargetPlayerIndex = State.TargetPlayerIndex;
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyNetworkReplicationProcessor.h"
#include "MassEnemyReplicationSubsystem.h"
#include "MyProjectGameState.h"
#include "MyProjectPlayerController.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UEnemyNetworkReplicationProcessor::UEnemyNetworkReplicationProcessor()
    : EntityQuery(*this)
{
	// Run on server only
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);

	// Run in PrePhysics phase (same as movement processor)
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// Auto-register with MASS
	bAutoRegisterWithProcessingPhases = true;
}

void UEnemyNetworkReplicationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Only configure if not already configured (prevents PIE crash)
	// if (EntityQuery.GetRequirements().IsEmpty())
	// {
		// Configure query - all alive enemies with network fragment
		EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyStateFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyMovementFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyAttackFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyNetworkFragment>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddRequirement<FEnemyTargetFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddTagRequirement<FEnemyTag>(EMassFragmentPresence::All);
		EntityQuery.AddTagRequirement<FEnemyDeadTag>(EMassFragmentPresence::None);  // Exclude dead enemies
	// }
}

void UEnemyNetworkReplicationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Only run on server
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// Get subsystem
	UMassEnemyReplicationSubsystem* ReplicationSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>();
	if (!ReplicationSubsystem)
	{
		return;
	}

	// Get all player controllers
	TArray<APlayerController*> AllPlayers = ReplicationSubsystem->GetAllPlayerControllers();
	if (AllPlayers.Num() == 0)
	{
		return;
	}

	// Build stable client indices (persistent across frames)
	for (APlayerController* PC : AllPlayers)
	{
		if (!ClientIndexMap.Contains(PC))
		{
			ClientIndexMap.Add(PC, NextClientIndex++);
		}
	}

	// Clean up disconnected clients from ClientIndexMap
	TArray<APlayerController*> DisconnectedClients;
	for (const auto& Pair : ClientIndexMap)
	{
		if (!AllPlayers.Contains(Pair.Key))
		{
			DisconnectedClients.Add(Pair.Key);
		}
	}
	for (APlayerController* DC : DisconnectedClients)
	{
		int32 ClientIdx = ClientIndexMap[DC];
		ClientIndexMap.Remove(DC);
		PerClientEntityTimers.Remove(ClientIdx);
	}

	// Initialize batches for each client
	TMap<APlayerController*, TArray<FCompressedEnemyState>> ClientBatches;
	for (APlayerController* PC : AllPlayers)
	{
		ClientBatches.Add(PC, TArray<FCompressedEnemyState>());
	}

	// Get current world time (absolute time, not delta)
	const float CurrentTime = World->GetTimeSeconds();

	// Precompute player locations to avoid repeated GetPawn() calls inside loop
	TMap<APlayerController*, FVector> PlayerLocations;
	for (APlayerController* PC : AllPlayers)
	{
		if (PC && PC->GetPawn())
		{
			PlayerLocations.Add(PC, PC->GetPawn()->GetActorLocation());
		}
	}

	// Process all entities
	EntityQuery.ForEachEntityChunk(Context, [&, this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FEnemyStateFragment> States = Context.GetFragmentView<FEnemyStateFragment>();
		const TConstArrayView<FEnemyMovementFragment> Movements = Context.GetFragmentView<FEnemyMovementFragment>();
		const TConstArrayView<FEnemyAttackFragment> Attacks = Context.GetFragmentView<FEnemyAttackFragment>();
		const TArrayView<FEnemyNetworkFragment> Networks = Context.GetMutableFragmentView<FEnemyNetworkFragment>();
		const TConstArrayView<FEnemyTargetFragment> Targets = Context.GetFragmentView<FEnemyTargetFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FTransformFragment& Transform = Transforms[EntityIndex];
			const FEnemyStateFragment& State = States[EntityIndex];
			const FEnemyMovementFragment& Movement = Movements[EntityIndex];
			const FEnemyAttackFragment& Attack = Attacks[EntityIndex];
			FEnemyNetworkFragment& Network = Networks[EntityIndex];

			// Assign NetworkID if needed
			if (Network.NetworkID == INDEX_NONE)
			{
				Network.NetworkID = ReplicationSubsystem->AssignNetworkID();
			}

			// Update network fragment data (position, rotation, health, flags, velocity)
			UpdateNetworkFragment(Transform, State, Movement, Attack, Network);

			FVector EntityLocation = Transform.GetTransform().GetLocation();
			bool bRelevantToAny = false;

			// Check EACH client independently - this is the key fix
			for (APlayerController* PC : AllPlayers)
			{
				// Get player location
				FVector* PlayerLocationPtr = PlayerLocations.Find(PC);
				if (!PlayerLocationPtr)
				{
					continue;
				}

				FVector PlayerLocation = *PlayerLocationPtr;
				float Distance = FVector::Dist(EntityLocation, PlayerLocation);

				// Check relevancy - skip if outside radius
				if (Distance > ReplicationSubsystem->GetRelevancyRadius())
				{
					continue;
				}

				bRelevantToAny = true;

				// Get required interval for THIS client based on distance
				float RequiredInterval = ReplicationSubsystem->GetReplicationInterval(Distance);

				// Get per-client, per-entity timer
				int32 ClientIdx = ClientIndexMap[PC];
				TMap<int32, float>& EntityTimers = PerClientEntityTimers.FindOrAdd(ClientIdx);
				float* LastSendTimePtr = EntityTimers.Find(Network.NetworkID);

				// Calculate time since last send to THIS client
				// If never sent, use large value to force immediate send
				float TimeSinceLastSend;
				if (LastSendTimePtr)
				{
					TimeSinceLastSend = CurrentTime - *LastSendTimePtr;
				}
				else
				{
					TimeSinceLastSend = RequiredInterval + 1.0f;  // Force first send
				}

				// Check if enough time has passed FOR THIS SPECIFIC CLIENT
				if (TimeSinceLastSend >= RequiredInterval)
				{
					// Calculate priority based on distance
					uint8 Priority = ReplicationSubsystem->CalculateReplicationPriority(EntityLocation, PlayerLocation);

					// Compress entity state
					FCompressedEnemyState CompressedState = CompressEntityState(Transform, State, Movement, Attack, Network);
					CompressedState.Priority = Priority;

					// Add to THIS client's batch
					ClientBatches[PC].Add(CompressedState);

					// Update timer ONLY for this client-entity pair (use [] to overwrite existing)
					EntityTimers.FindOrAdd(Network.NetworkID) = CurrentTime;
				}
			}

			Network.bIsRelevantToAnyClient = bRelevantToAny;
		}
	});

	// Sort each client's batch by priority (highest priority first)
	for (auto& Pair : ClientBatches)
	{
		Pair.Value.Sort([](const FCompressedEnemyState& A, const FCompressedEnemyState& B)
		{
			return A.Priority > B.Priority;  // Descending - higher priority first
		});
	}

	// Log batch collection summary with details per client
	int32 TotalEntities = 0;
	for (const auto& Pair : ClientBatches)
	{
		TotalEntities += Pair.Value.Num();
		if (Pair.Value.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION-LAG] Server: Client %s - queued %d entities"),
				*Pair.Key->GetName(), Pair.Value.Num());
		}
	}

	if (TotalEntities > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION-LAG] Server: Total %d entities for %d clients"),
			TotalEntities, ClientBatches.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION-LAG] Server: NO entities collected! Check relevancy/timing logic"));
	}

	// Send batches to clients
	SendBatchesToClients(ClientBatches);
}

void UEnemyNetworkReplicationProcessor::UpdateNetworkFragment(
	const FTransformFragment& Transform,
	const FEnemyStateFragment& State,
	const FEnemyMovementFragment& Movement,
	const FEnemyAttackFragment& Attack,
	FEnemyNetworkFragment& Network)
{
	// NOTE: No timer logic here anymore - timers are handled per-client in Execute()

	// Update position
	FVector Location = Transform.GetTransform().GetLocation();
	Network.ReplicatedPosition = Location;

	// Compress rotation (yaw only, 16-bit)
	FRotator Rotation = Transform.GetTransform().GetRotation().Rotator();
	float NormalizedYaw = (Rotation.Yaw + 180.0f) / 360.0f;  // Map -180..180 to 0..1
	Network.ReplicatedRotationYaw = static_cast<uint16>(NormalizedYaw * 65535.0f);

	// Compress health (0-100 to 0-255)
	Network.ReplicatedHealth = static_cast<uint8>(FMath::Clamp(State.Health / 100.0f * 255.0f, 0.0f, 255.0f));

	// Pack boolean flags into single byte
	Network.ReplicatedFlags = 0;
	if (State.bIsAlive)
		Network.ReplicatedFlags |= (1 << 0);
	if (Attack.bIsAttacking)
		Network.ReplicatedFlags |= (1 << 1);
	if (State.bIsMoving)
		Network.ReplicatedFlags |= (1 << 2);

	// Store velocity for client-side prediction
	Network.ReplicatedVelocity = Movement.Velocity;

	// Target player index (set elsewhere if needed)
	Network.TargetPlayerIndex = -1;
}

FCompressedEnemyState UEnemyNetworkReplicationProcessor::CompressEntityState(
	const FTransformFragment& Transform,
	const FEnemyStateFragment& State,
	const FEnemyMovementFragment& Movement,
	const FEnemyAttackFragment& Attack,
	const FEnemyNetworkFragment& Network)
{
	FCompressedEnemyState CompressedState;
	CompressedState.NetworkID = Network.NetworkID;
	CompressedState.Position = Network.ReplicatedPosition;
	CompressedState.RotationYaw = Network.ReplicatedRotationYaw;
	CompressedState.Health = Network.ReplicatedHealth;
	CompressedState.Flags = Network.ReplicatedFlags;
	CompressedState.Velocity = Network.ReplicatedVelocity;
	CompressedState.TargetPlayerIndex = Network.TargetPlayerIndex;

	return CompressedState;
}

void UEnemyNetworkReplicationProcessor::SendBatchesToClients(const TMap<APlayerController*, TArray<FCompressedEnemyState>>& ClientBatches)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] SendBatchesToClients: No World!"));
		return;
	}

	// Get replication subsystem
	UMassEnemyReplicationSubsystem* RepSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>();
	if (!RepSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] SendBatchesToClients: No ReplicationSubsystem!"));
		return;
	}

	// Queue batches for sending on game thread
	// This is thread-safe and can be called from MASS worker thread
	for (const auto& Pair : ClientBatches)
	{
		APlayerController* Client = Pair.Key;
		const TArray<FCompressedEnemyState>& Entities = Pair.Value;

		if (!Client || Entities.Num() == 0)
			continue;

		int32 BatchCount = 0;
		// Split into batches of MaxEntitiesPerBatch
		for (int32 StartIndex = 0; StartIndex < Entities.Num(); StartIndex += MaxEntitiesPerBatch)
		{
			int32 EndIndex = FMath::Min(StartIndex + MaxEntitiesPerBatch, Entities.Num());

			FMassEntityBatchUpdate Batch;
			for (int32 i = StartIndex; i < EndIndex; ++i)
			{
				Batch.Entities.Add(Entities[i]);
			}

			// Queue for sending on game thread (thread-safe)
			RepSubsystem->QueueBatchForSending(Client, Batch);
			BatchCount++;
		}

		UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION] Server: Queued %d batches (%d entities) for client %s"),
			BatchCount, Entities.Num(), *Client->GetName());
	}
}

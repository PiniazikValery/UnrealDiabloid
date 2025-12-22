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
{
	// Run on server only
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);

	// Run in PrePhysics phase (same as movement processor)
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// Auto-register with MASS
	bAutoRegisterWithProcessingPhases = true;
}

void UEnemyNetworkReplicationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Only configure if not already configured (prevents PIE crash)
	if (EntityQuery.GetRequirements().IsEmpty())
	{
		// Configure query - all alive enemies with network fragment
		EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyStateFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyMovementFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyAttackFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FEnemyNetworkFragment>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddRequirement<FEnemyTargetFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddTagRequirement<FEnemyTag>(EMassFragmentPresence::All);
		EntityQuery.AddTagRequirement<FEnemyDeadTag>(EMassFragmentPresence::None);  // Exclude dead enemies
	}
}

void UEnemyNetworkReplicationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Get subsystem
	UMassEnemyReplicationSubsystem* ReplicationSubsystem = GetWorld()->GetSubsystem<UMassEnemyReplicationSubsystem>();
	if (!ReplicationSubsystem)
	{
		return;
	}

	// Get GameState for sending RPCs
	AMyProjectGameState* GameState = GetWorld()->GetGameState<AMyProjectGameState>();
	if (!GameState)
	{
		return;
	}

	// Get all player controllers
	TArray<APlayerController*> AllPlayers = ReplicationSubsystem->GetAllPlayerControllers();
	if (AllPlayers.Num() == 0)
	{
		return;  // No clients to replicate to
	}

	// Per-client batches
	TMap<APlayerController*, TArray<FCompressedEnemyState>> ClientBatches;

	// Initialize batches for each client
	for (APlayerController* PC : AllPlayers)
	{
		ClientBatches.Add(PC, TArray<FCompressedEnemyState>());
	}

	float DeltaTime = Context.GetDeltaTimeSeconds();

	// Process entities
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
			const FEnemyTargetFragment& Target = Targets[EntityIndex];

			// Assign NetworkID if needed
			if (Network.NetworkID == INDEX_NONE)
			{
				Network.NetworkID = ReplicationSubsystem->AssignNetworkID();
			}

			// Update network fragment
			UpdateNetworkFragment(Transform, State, Movement, Attack, Network, DeltaTime);

			// Check relevancy to each client
			TArray<APlayerController*> RelevantPlayers;
			FVector EntityLocation = Transform.GetTransform().GetLocation();

			if (ReplicationSubsystem->IsEntityRelevant(EntityLocation, RelevantPlayers))
			{
				Network.bIsRelevantToAnyClient = true;

				// For each relevant client, check if update is needed
				for (APlayerController* PC : RelevantPlayers)
				{
					if (!PC || !PC->GetPawn())
						continue;

					FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
					float Distance = FVector::Dist(EntityLocation, PlayerLocation);
					float RequiredInterval = ReplicationSubsystem->GetReplicationInterval(Distance);

					// Check if enough time has passed since last replication
					if (Network.TimeSinceLastReplication >= RequiredInterval)
					{
						// Update priority
						Network.ReplicationPriority = ReplicationSubsystem->CalculateReplicationPriority(EntityLocation, PlayerLocation);

						// Add to client's batch
						FCompressedEnemyState CompressedState = CompressEntityState(Transform, State, Movement, Attack, Network);
						ClientBatches[PC].Add(CompressedState);
					}
				}

				// Reset timer if we replicated to at least one client
				if (Network.TimeSinceLastReplication >= ReplicationSubsystem->GetReplicationInterval(0.0f))
				{
					Network.TimeSinceLastReplication = 0.0f;
				}
			}
			else
			{
				Network.bIsRelevantToAnyClient = false;
			}
		}
	});

	// Send batches to clients
	SendBatchesToClients(ClientBatches);
}

void UEnemyNetworkReplicationProcessor::UpdateNetworkFragment(
	const FTransformFragment& Transform,
	const FEnemyStateFragment& State,
	const FEnemyMovementFragment& Movement,
	const FEnemyAttackFragment& Attack,
	FEnemyNetworkFragment& Network,
	float DeltaTime)
{
	// Update timer
	Network.TimeSinceLastReplication += DeltaTime;

	// Update compressed state
	FVector Location = Transform.GetTransform().GetLocation();
	Network.ReplicatedPosition = Location;

	// Compress rotation (yaw only, 16-bit)
	FRotator Rotation = Transform.GetTransform().GetRotation().Rotator();
	float NormalizedYaw = (Rotation.Yaw + 180.0f) / 360.0f;  // Map -180..180 to 0..1
	Network.ReplicatedRotationYaw = static_cast<uint16>(NormalizedYaw * 65535.0f);

	// Compress health (0-100 to 0-255)
	Network.ReplicatedHealth = static_cast<uint8>(FMath::Clamp(State.Health / 100.0f * 255.0f, 0.0f, 255.0f));

	// Pack flags
	Network.ReplicatedFlags = 0;
	if (State.bIsAlive)
		Network.ReplicatedFlags |= (1 << 0);
	if (Attack.bIsAttacking)
		Network.ReplicatedFlags |= (1 << 1);
	if (State.bIsMoving)
		Network.ReplicatedFlags |= (1 << 2);

	// Store velocity for client prediction
	Network.ReplicatedVelocity = Movement.Velocity;

	// Target player index will be set in Phase 2
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
		return;
	}

	// Send batches to each client via their PlayerController
	for (const auto& Pair : ClientBatches)
	{
		APlayerController* Client = Pair.Key;
		const TArray<FCompressedEnemyState>& Entities = Pair.Value;

		if (!Client || Entities.Num() == 0)
			continue;

		// Cast to our custom PlayerController
		AMyProjectPlayerController* MyPC = Cast<AMyProjectPlayerController>(Client);
		if (!MyPC)
			continue;

		// Split into batches of MaxEntitiesPerBatch
		for (int32 StartIndex = 0; StartIndex < Entities.Num(); StartIndex += MaxEntitiesPerBatch)
		{
			int32 EndIndex = FMath::Min(StartIndex + MaxEntitiesPerBatch, Entities.Num());

			FMassEntityBatchUpdate Batch;
			for (int32 i = StartIndex; i < EndIndex; ++i)
			{
				Batch.Entities.Add(Entities[i]);
			}

			// Send RPC to this specific client via PlayerController
			MyPC->ClientReceiveMassEntityBatch(Batch);
		}
	}
}

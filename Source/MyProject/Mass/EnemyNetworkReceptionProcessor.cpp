// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyNetworkReceptionProcessor.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassEnemyReplicationSubsystem.h"
#include "MyProjectPlayerController.h"
#include "Engine/World.h"
#include "EnemyTrait.h"
#include "GameFramework/PlayerController.h"

UEnemyNetworkReceptionProcessor::UEnemyNetworkReceptionProcessor()
{
	// Run on clients only
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client);

	// Run in PrePhysics phase (before visualization)
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// Auto-register with MASS
	bAutoRegisterWithProcessingPhases = true;

	// This processor doesn't use entity queries - it creates/updates entities directly via RPC data
	// No query configuration needed
}

void UEnemyNetworkReceptionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);
}

void UEnemyNetworkReceptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Process batch updates from GameState
	ProcessBatchUpdates(EntityManager);
}

void UEnemyNetworkReceptionProcessor::ProcessBatchUpdates(FMassEntityManager& EntityManager)
{
	// Get the replication subsystem
	UMassEnemyReplicationSubsystem* RepSubsystem = GetWorld()->GetSubsystem<UMassEnemyReplicationSubsystem>();
	if (!RepSubsystem)
	{
		return;
	}

	// Get local player controller
	APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	if (!LocalPC)
	{
		return;
	}

	// Try to get batch data from subsystem
	FMassEntityBatchUpdate BatchData;
	if (!RepSubsystem->GetAndClearBatchForClient(LocalPC, BatchData))
	{
		// No batch data available this frame
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("EnemyNetworkReceptionProcessor: Processing %d entities"), BatchData.Entities.Num());

	// Process each entity in the batch
	for (const FCompressedEnemyState& State : BatchData.Entities)
	{
		// Check if we already have this entity
		if (FMassEntityHandle* ExistingEntity = NetworkIDToEntity.Find(State.NetworkID))
		{
			// Update existing entity
			if (EntityManager.IsEntityValid(*ExistingEntity))
			{
				UpdateClientEntity(EntityManager, *ExistingEntity, State);
			}
			else
			{
				// Entity is invalid, remove from map and create new one
				NetworkIDToEntity.Remove(State.NetworkID);
				CreateClientEntity(EntityManager, State);
			}
		}
		else
		{
			// Create new entity
			CreateClientEntity(EntityManager, State);
		}
	}
}

FMassEntityHandle UEnemyNetworkReceptionProcessor::CreateClientEntity(
	FMassEntityManager& EntityManager,
	const FCompressedEnemyState& State)
{
	// Create entity with all required fragments for client-side shadow entity
	FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(
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

	FMassEntityHandle NewEntity = EntityManager.CreateEntity(Archetype);

	if (!EntityManager.IsEntityValid(NewEntity))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create client entity for NetworkID %d"), State.NetworkID);
		return FMassEntityHandle();
	}

	// Set initial state from network data
	UpdateClientEntity(EntityManager, NewEntity, State);

	// Store mapping
	NetworkIDToEntity.Add(State.NetworkID, NewEntity);

	UE_LOG(LogTemp, Log, TEXT("Created client entity for NetworkID %d at %s"),
		State.NetworkID, *State.Position.ToString());

	return NewEntity;
}

void UEnemyNetworkReceptionProcessor::UpdateClientEntity(
	FMassEntityManager& EntityManager,
	FMassEntityHandle EntityHandle,
	const FCompressedEnemyState& State)
{
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return;
	}

	// Update Transform
	FTransformFragment& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(EntityHandle);
	FVector Position = State.Position;
	float Yaw = DecompressRotationYaw(State.RotationYaw);
	FRotator Rotation(0.0f, Yaw, 0.0f);
	Transform.SetTransform(FTransform(Rotation, Position, FVector::OneVector));

	// Update State
	FEnemyStateFragment& EnemyState = EntityManager.GetFragmentDataChecked<FEnemyStateFragment>(EntityHandle);
	EnemyState.Health = DecompressHealth(State.Health);
	EnemyState.bIsAlive = State.IsAlive();
	EnemyState.bIsMoving = State.IsMoving();

	// Update Movement (velocity for prediction)
	FEnemyMovementFragment& Movement = EntityManager.GetFragmentDataChecked<FEnemyMovementFragment>(EntityHandle);
	Movement.Velocity = State.Velocity;

	// Update Attack
	FEnemyAttackFragment& Attack = EntityManager.GetFragmentDataChecked<FEnemyAttackFragment>(EntityHandle);
	Attack.bIsAttacking = State.IsAttacking();

	// Update Network fragment
	FEnemyNetworkFragment& Network = EntityManager.GetFragmentDataChecked<FEnemyNetworkFragment>(EntityHandle);
	Network.NetworkID = State.NetworkID;
	Network.ReplicatedPosition = State.Position;
	Network.ReplicatedRotationYaw = State.RotationYaw;
	Network.ReplicatedHealth = State.Health;
	Network.ReplicatedFlags = State.Flags;
	Network.ReplicatedVelocity = State.Velocity;
	Network.TargetPlayerIndex = State.TargetPlayerIndex;

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated client entity NetworkID %d at %s"),
		State.NetworkID, *State.Position.ToString());
}

float UEnemyNetworkReceptionProcessor::DecompressRotationYaw(uint16 CompressedYaw) const
{
	// Convert 0-65535 back to -180..180
	float Normalized = static_cast<float>(CompressedYaw) / 65535.0f;  // 0..1
	float Yaw = (Normalized * 360.0f) - 180.0f;  // -180..180
	return Yaw;
}

float UEnemyNetworkReceptionProcessor::DecompressHealth(uint8 CompressedHealth) const
{
	// Convert 0-255 back to 0-100
	return (static_cast<float>(CompressedHealth) / 255.0f) * 100.0f;
}

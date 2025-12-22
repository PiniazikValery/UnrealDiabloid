// Fill out your copyright notice in the Description page of Project Settings.

#include "MyProjectGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AMyProjectGameState::AMyProjectGameState()
{
	// Enable replication
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AMyProjectGameState::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("MyProjectGameState: BeginPlay (Role: %s)"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

void AMyProjectGameState::ClientReceiveMassEntityBatch_Implementation(const FMassEntityBatchUpdate& BatchData)
{
	// This runs on clients only
	// The reception processor will pick up this data and update local entities
	// For now, just log for verification
	UE_LOG(LogTemp, Verbose, TEXT("ClientReceiveMassEntityBatch: Received %d entities"), BatchData.Entities.Num());

	// Note: The actual processing will be done by UEnemyNetworkReceptionProcessor
	// which will query this data or be notified via a subsystem
	// For Phase 1, we'll store this in the replication subsystem for the processor to consume
}

void AMyProjectGameState::ClientNotifyEnemySpawn_Implementation(int32 NetworkID, FVector Location)
{
	// Spawn notification for clients
	UE_LOG(LogTemp, Log, TEXT("ClientNotifyEnemySpawn: NetworkID=%d at %s"), NetworkID, *Location.ToString());

	// The reception processor will create the local entity
	// This will be implemented in the reception processor
}

void AMyProjectGameState::MulticastEnemyDeath_Implementation(int32 NetworkID, FVector Location)
{
	// Death notification to all clients
	UE_LOG(LogTemp, Log, TEXT("MulticastEnemyDeath: NetworkID=%d at %s"), NetworkID, *Location.ToString());

	// The reception processor will destroy the local entity
	// This will be implemented when we add death synchronization in Phase 5
}

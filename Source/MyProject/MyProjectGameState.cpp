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
	// The replication subsystem will process this data and create/update client entities
	UE_LOG(LogTemp, Verbose, TEXT("ClientReceiveMassEntityBatch: Received %d entities"), BatchData.Entities.Num());

	// Store in the replication subsystem for processing in ProcessClientReception()
}

void AMyProjectGameState::ClientNotifyEnemySpawn_Implementation(int32 NetworkID, FVector Location)
{
	// Spawn notification for clients
	UE_LOG(LogTemp, Log, TEXT("ClientNotifyEnemySpawn: NetworkID=%d at %s"), NetworkID, *Location.ToString());

	// The replication subsystem will create the local entity
	// This will be implemented in the subsystem's ProcessClientReception()
}

void AMyProjectGameState::MulticastEnemyDeath_Implementation(int32 NetworkID, FVector Location)
{
	// Death notification to all clients
	UE_LOG(LogTemp, Log, TEXT("MulticastEnemyDeath: NetworkID=%d at %s"), NetworkID, *Location.ToString());

	// The replication subsystem will destroy the local entity
	// This will be implemented when we add death synchronization
}

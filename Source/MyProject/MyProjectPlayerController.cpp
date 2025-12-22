// Fill out your copyright notice in the Description page of Project Settings.

#include "MyProjectPlayerController.h"
#include "Mass/MassEnemyReplicationSubsystem.h"

void AMyProjectPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("MyProjectPlayerController: BeginPlay (Role: %s)"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

void AMyProjectPlayerController::ClientReceiveMassEntityBatch_Implementation(const FMassEntityBatchUpdate& BatchData)
{
	// This runs on the client only for THIS specific player controller
	UE_LOG(LogTemp, Verbose, TEXT("ClientReceiveMassEntityBatch: Received %d entities"), BatchData.Entities.Num());

	// Store batch data in the client-side replication subsystem for processing
	// The reception processor will consume this data
	if (UWorld* World = GetWorld())
	{
		if (UMassEnemyReplicationSubsystem* RepSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>())
		{
			RepSubsystem->StoreBatchForClient(this, BatchData);
		}
	}
}

void AMyProjectPlayerController::ClientNotifyEnemySpawn_Implementation(int32 NetworkID, FVector Location)
{
	// Spawn notification for this client
	UE_LOG(LogTemp, Log, TEXT("ClientNotifyEnemySpawn: NetworkID=%d at %s"), NetworkID, *Location.ToString());

	// The reception processor will create the local entity
	// This will be implemented in the reception processor
}

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
	UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION] Client RPC: Received batch with %d entities"), BatchData.Entities.Num());

	// Store batch data in the client-side replication subsystem for processing
	// The reception processor will consume this data
	if (UWorld* World = GetWorld())
	{
		if (UMassEnemyReplicationSubsystem* RepSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>())
		{
			RepSubsystem->StoreBatchForClient(this, BatchData);
			UE_LOG(LogTemp, Log, TEXT("[MASS-REPLICATION] Client RPC: Stored batch in subsystem"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Client RPC: No ReplicationSubsystem!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Client RPC: No World!"));
	}
}

void AMyProjectPlayerController::ClientNotifyEnemySpawn_Implementation(int32 NetworkID, FVector Location)
{
	// Spawn notification for this client
	UE_LOG(LogTemp, Log, TEXT("ClientNotifyEnemySpawn: NetworkID=%d at %s"), NetworkID, *Location.ToString());

	// The reception processor will create the local entity
	// This will be implemented in the reception processor
}

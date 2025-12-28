// Fill out your copyright notice in the Description page of Project Settings.

#include "MyProjectPlayerController.h"
#include "Mass/MassEnemyReplicationSubsystem.h"
#include "AutoAimHelper.h"

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

void AMyProjectPlayerController::ClientReceiveDeathNotifications_Implementation(const TArray<int32>& NetworkIDs)
{
	UE_LOG(LogTemp, Warning, TEXT("[MASS-REPLICATION] Client RPC: Received %d death notifications"), NetworkIDs.Num());

	if (UWorld* World = GetWorld())
	{
		if (UMassEnemyReplicationSubsystem* RepSubsystem = World->GetSubsystem<UMassEnemyReplicationSubsystem>())
		{
			RepSubsystem->HandleDeathNotifications(NetworkIDs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MASS-REPLICATION] Client RPC: No ReplicationSubsystem for death notifications!"));
		}
	}
}

void AMyProjectPlayerController::ServerApplyDamageToMassEntity_Implementation(int32 TargetNetworkID, float Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("[MASS-DAMAGE] Server RPC: Applying %.1f damage to NetworkID %d"), Damage, TargetNetworkID);

	// Apply damage on the server (authoritative)
	bool bSuccess = UAutoAimHelper::ApplyDamageToMassEntity(this, TargetNetworkID, Damage);

	UE_LOG(LogTemp, Warning, TEXT("[MASS-DAMAGE] Server RPC: Damage application %s for NetworkID %d"),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"), TargetNetworkID);
}

void AMyProjectPlayerController::ServerApplyDamageAtLocation_Implementation(FVector HitLocation, float DamageRadius, float Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("[MASS-DAMAGE] Server RPC: Applying %.1f area damage at %s (radius %.1f)"),
		Damage, *HitLocation.ToString(), DamageRadius);

	// Apply area damage on the server (authoritative)
	int32 DamagedCount = UAutoAimHelper::ApplyDamageAtLocation(this, HitLocation, DamageRadius, Damage);

	UE_LOG(LogTemp, Warning, TEXT("[MASS-DAMAGE] Server RPC: Area damage hit %d enemies"), DamagedCount);
}

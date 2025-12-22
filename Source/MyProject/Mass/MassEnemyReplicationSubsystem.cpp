// Fill out your copyright notice in the Description page of Project Settings.

#include "MassEnemyReplicationSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UMassEnemyReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("MassEnemyReplicationSubsystem: Initialized"));
}

void UMassEnemyReplicationSubsystem::Deinitialize()
{
	Super::Deinitialize();

	PendingClientBatches.Empty();
	ReleasedNetworkIDs.Empty();

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
	// Reuse released IDs if available
	if (ReleasedNetworkIDs.Num() > 0)
	{
		int32 ReusedID = INDEX_NONE;
		for (int32 ID : ReleasedNetworkIDs)
		{
			ReusedID = ID;
			break;
		}
		ReleasedNetworkIDs.Remove(ReusedID);
		return ReusedID;
	}

	// Otherwise, assign next sequential ID
	return NextNetworkID++;
}

void UMassEnemyReplicationSubsystem::ReleaseNetworkID(int32 NetworkID)
{
	if (NetworkID != INDEX_NONE)
	{
		ReleasedNetworkIDs.Add(NetworkID);
	}
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

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MyProjectGameState.generated.h"

/**
 * Compressed enemy state for network transmission
 * Optimized for bandwidth (approximately 33 bytes per enemy)
 */
USTRUCT()
struct FCompressedEnemyState
{
	GENERATED_BODY()

	// Network ID for entity mapping
	UPROPERTY()
	int32 NetworkID = INDEX_NONE;

	// Position (10cm precision for bandwidth savings)
	UPROPERTY()
	FVector_NetQuantize10 Position = FVector::ZeroVector;

	// Rotation (yaw only, 16-bit for bandwidth)
	UPROPERTY()
	uint16 RotationYaw = 0;

	// Health (0-255, scaled from 0-100)
	UPROPERTY()
	uint8 Health = 255;

	// Bit-packed flags: bIsAlive(bit 0), bIsAttacking(bit 1), bIsMoving(bit 2)
	UPROPERTY()
	uint8 Flags = 0;

	// Velocity for client prediction
	UPROPERTY()
	FVector_NetQuantize Velocity = FVector::ZeroVector;

	// Target player index (-1 = no target)
	UPROPERTY()
	int16 TargetPlayerIndex = -1;

	// Helper functions for flag manipulation
	bool IsAlive() const { return (Flags & (1 << 0)) != 0; }
	bool IsAttacking() const { return (Flags & (1 << 1)) != 0; }
	bool IsMoving() const { return (Flags & (1 << 2)) != 0; }

	void SetAlive(bool bAlive)
	{
		if (bAlive)
			Flags |= (1 << 0);
		else
			Flags &= ~(1 << 0);
	}

	void SetAttacking(bool bAttacking)
	{
		if (bAttacking)
			Flags |= (1 << 1);
		else
			Flags &= ~(1 << 1);
	}

	void SetMoving(bool bMoving)
	{
		if (bMoving)
			Flags |= (1 << 2);
		else
			Flags &= ~(1 << 2);
	}
};

/**
 * Batch update containing multiple enemy states
 * Sent in a single RPC to minimize network overhead
 */
USTRUCT()
struct FMassEntityBatchUpdate
{
	GENERATED_BODY()

	// Array of compressed enemy states (up to 50 per batch for optimal packet size)
	UPROPERTY()
	TArray<FCompressedEnemyState> Entities;
};

/**
 * Game State for MyProject
 * Handles MASS entity replication via batch RPCs
 */
UCLASS()
class MYPROJECT_API AMyProjectGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMyProjectGameState();

	/**
	 * Client RPC to receive batch of MASS entity updates
	 * Called by server's replication processor
	 * Unreliable for performance (position updates can tolerate occasional loss)
	 */
	UFUNCTION(Client, Unreliable)
	void ClientReceiveMassEntityBatch(const FMassEntityBatchUpdate& BatchData);

	/**
	 * Client RPC for reliable entity spawn notifications
	 * Ensures clients create entities when server spawns them
	 */
	UFUNCTION(Client, Reliable)
	void ClientNotifyEnemySpawn(int32 NetworkID, FVector Location);

	/**
	 * Client RPC for reliable entity death notifications
	 * Ensures immediate feedback for enemy deaths
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEnemyDeath(int32 NetworkID, FVector Location);

protected:
	virtual void BeginPlay() override;
};

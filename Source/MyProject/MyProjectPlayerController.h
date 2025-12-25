// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyProjectGameState.h"
#include "MyProjectPlayerController.generated.h"

/**
 * Custom PlayerController for MyProject
 * Handles client-side reception of MASS entity updates via RPC
 */
UCLASS()
class MYPROJECT_API AMyProjectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/**
	 * Client RPC to receive batch of MASS entity updates
	 * Called by server's replication processor for this specific client
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

protected:
	virtual void BeginPlay() override;
};

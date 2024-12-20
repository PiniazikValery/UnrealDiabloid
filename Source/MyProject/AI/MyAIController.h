// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DetourCrowdAIController.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../MyProjectCharacter.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "MyAIController.generated.h"

/**
 *
 */
UCLASS()
class MYPROJECT_API AMyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMyAIController(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	void		 MoveToPlayer();

	FTimerHandle TimerHandle_MoveToActor;

protected:
	APawn*				 PlayerPawn;
	AMyProjectCharacter* Agent;
	FVector				 PreviousPlayerLocation;
	FVector				 PreviousAgentLocation;

private:
	bool follow = false;
	bool bIsInitialTick = true;
};

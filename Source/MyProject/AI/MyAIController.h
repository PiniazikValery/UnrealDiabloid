// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DetourCrowdAIController.h"
#include "Navigation/CrowdFollowingComponent.h"
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
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	void		 FollowPlayer();
	void		 MoveToPlayer();

	FTimerHandle TimerHandle_MoveToActor;
	float		 MoveDelay;
	bool		 AITryingToReach = false;

protected:
	APawn* PlayerPawn;

private:
	bool follow = false;
};

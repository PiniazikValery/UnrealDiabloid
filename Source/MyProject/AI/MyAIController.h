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
#include "../EnemyCharacter.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "MyAIController.generated.h"

/**
 *
 */
UCLASS()
class AMyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMyAIController(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
	UPROPERTY()
	AMyProjectCharacter* Agent = nullptr;

	UPROPERTY()
	APawn* PlayerPawn = nullptr;

	FVector PreviousPlayerLocation = FVector::ZeroVector;
	FVector LocationToMove = FVector::ZeroVector;

	bool  bIsInAttackRange = false;
	float TimeSinceLastMoveRequest = 0.f;
	float TimeSinceLastAttack = 0.f;

	void MoveToPlayer();
	void PerformAttack();
};
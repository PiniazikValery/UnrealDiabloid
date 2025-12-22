// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassCommonFragments.h"
#include "EnemyFragments.h"
#include "EnemyAttackProcessor.generated.h"

/**
 * Processor that handles enemy attack logic
 * This replaces the attack detection and PerformAttack() logic from your old AIController
 * 
 * Execution: Every frame in Tasks phase
 */
UCLASS()
class MYPROJECT_API UEnemyAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UEnemyAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	
	// Helper function to perform attack (equivalent to your PerformAttack())
	void ExecuteAttack(const FVector& AttackerLocation, const FVector& TargetLocation, 
					  float Damage, AActor* TargetActor, UWorld* World);
};

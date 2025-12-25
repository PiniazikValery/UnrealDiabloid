// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "EnemyFragments.h"
#include "MassCommonFragments.h"
#include "EnemyClientInterpolationProcessor.generated.h"

/**
 * Client-side processor that smoothly interpolates entity positions
 * Runs every frame to eliminate teleporting between server updates
 *
 * Execution: Client only, PrePhysics phase (before visualization)
 */
UCLASS()
class MYPROJECT_API UEnemyClientInterpolationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UEnemyClientInterpolationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	// Interpolation settings
	UPROPERTY(EditAnywhere, Category = "Interpolation")
	float InterpolationSpeed = 15.0f;  // How fast to catch up

	UPROPERTY(EditAnywhere, Category = "Interpolation")
	float MaxExtrapolationTime = 0.2f;  // Max time to predict ahead

	UPROPERTY(EditAnywhere, Category = "Interpolation")
	float TeleportThreshold = 500.0f;  // Distance to snap instead of interpolate
};

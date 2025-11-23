// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AutoAimHelper.generated.h"

class AEnemyCharacter;

UENUM(BlueprintType)
enum class ETargetSelectionMode : uint8
{
	ClosestToCenter UMETA(DisplayName = "Closest to Center"),
	ClosestByDistance UMETA(DisplayName = "Closest by Distance"),
	LowestHealth UMETA(DisplayName = "Lowest Health"),
	HighestThreat UMETA(DisplayName = "Highest Threat")
};

USTRUCT(BlueprintType)
struct FAutoAimResult
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	AActor* Target = nullptr;
	
	UPROPERTY(BlueprintReadOnly)
	float AimAngle = 0.f;
	
	UPROPERTY(BlueprintReadOnly)
	float DistanceToTarget = 0.f;
	
	UPROPERTY(BlueprintReadOnly)
	bool bTargetFound = false;
};

/**
 * Helper class for calculating auto-aim angles towards enemies
 * Can be used by both player characters and AI
 */
UCLASS(Blueprintable, BlueprintType)
class MYPROJECT_API UAutoAimHelper : public UObject
{
	GENERATED_BODY()
	
public:
	UAutoAimHelper();
	
	/**
	 * Find the best target and calculate aim angle
	 * @param SourceActor - The actor doing the aiming (player or AI)
	 * @param TargetClass - Class to search for (default: AEnemyCharacter)
	 * @return Struct containing target info and aim angle
	 */
	UFUNCTION(BlueprintCallable, Category = "Auto-Aim")
	static FAutoAimResult FindBestTargetAndAngle(
		AActor* SourceActor,
		TSubclassOf<AActor> TargetClass = nullptr,
		float SearchRange = 1000.f,
		float MaxAngleDegrees = 90.f,
		ETargetSelectionMode SelectionMode = ETargetSelectionMode::ClosestToCenter
	);
	
	/**
	 * Calculate the angle to aim at a specific target
	 * @param SourceActor - The actor doing the aiming
	 * @param Target - The target to aim at
	 * @return Angle in degrees relative to source's forward direction
	 */
	UFUNCTION(BlueprintCallable, Category = "Auto-Aim")
	static float CalculateAimAngleToTarget(AActor* SourceActor, AActor* Target);
	
	/**
	 * Check if a target is within the front arc
	 * @param SourceActor - The actor doing the checking
	 * @param TargetLocation - Location to check
	 * @param MaxAngleDegrees - Maximum angle from forward (90 = 180 degree arc)
	 * @param OutAngle - The calculated angle to the target
	 * @return True if target is within arc
	 */
	UFUNCTION(BlueprintCallable, Category = "Auto-Aim")
	static bool IsTargetInFrontArc(
		AActor* SourceActor,
		FVector TargetLocation,
		float MaxAngleDegrees,
		float& OutAngle
	);
	
	/**
	 * Find all valid targets within range and arc
	 */
	static TArray<AActor*> FindTargetsInArc(
		AActor* SourceActor,
		TSubclassOf<AActor> TargetClass,
		float SearchRange,
		float MaxAngleDegrees
	);
	
private:
	static float CalculateTargetScore(
		AActor* SourceActor,
		AActor* Target,
		float AngleToTarget,
		ETargetSelectionMode SelectionMode
	);
};

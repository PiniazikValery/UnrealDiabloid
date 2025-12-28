// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MassEntityTypes.h"
#include "AutoAimHelper.generated.h"

class AEnemyCharacter;
class UMassEntitySubsystem;

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
 * Result struct for Mass Entity auto-aim (entities don't have AActor pointers)
 */
USTRUCT(BlueprintType)
struct FMassAutoAimResult
{
	GENERATED_BODY()

	// Entity handle (only valid during the frame it was found)
	FMassEntityHandle EntityHandle;

	// World position of the target
	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float AimAngle = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float DistanceToTarget = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bTargetFound = false;

	// Network ID of the target entity (useful for tracking across frames)
	UPROPERTY(BlueprintReadOnly)
	int32 TargetNetworkID = INDEX_NONE;
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

	/**
	 * Find the best Mass Entity target and calculate aim angle
	 * This is for MASS-based enemies that don't have AActor representations
	 * @param SourceActor - The actor doing the aiming (player)
	 * @param SearchRange - Maximum distance to search for targets
	 * @param MaxAngleDegrees - Maximum angle from forward direction (90 = 180 degree arc)
	 * @param SelectionMode - How to prioritize targets
	 * @param bCheckVisibility - Whether to perform line trace visibility check
	 * @return Struct containing target info and aim angle
	 */
	UFUNCTION(BlueprintCallable, Category = "Auto-Aim")
	static FMassAutoAimResult FindBestMassEntityTarget(
		AActor* SourceActor,
		float SearchRange = 1000.f,
		float MaxAngleDegrees = 90.f,
		ETargetSelectionMode SelectionMode = ETargetSelectionMode::ClosestByDistance,
		bool bCheckVisibility = true
	);

	/**
	 * Calculate the angle to aim at a specific location
	 * @param SourceActor - The actor doing the aiming
	 * @param TargetLocation - The location to aim at
	 * @return Angle in degrees relative to source's forward direction
	 */
	UFUNCTION(BlueprintCallable, Category = "Auto-Aim")
	static float CalculateAimAngleToLocation(AActor* SourceActor, FVector TargetLocation);

	/**
	 * Apply damage to a Mass Entity enemy by NetworkID
	 * @param WorldContext - Any actor in the world (for getting subsystems)
	 * @param TargetNetworkID - The network ID of the target entity
	 * @param Damage - Amount of damage to apply
	 * @return True if damage was applied successfully, false if entity not found or already dead
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Entity Damage")
	static bool ApplyDamageToMassEntity(AActor* WorldContext, int32 TargetNetworkID, float Damage);

	/**
	 * Apply damage to a Mass Entity enemy at a specific location (sphere check)
	 * Useful for projectiles that hit a location rather than having a specific target
	 * @param WorldContext - Any actor in the world (for getting subsystems)
	 * @param HitLocation - The location where damage should be applied
	 * @param DamageRadius - Radius to check for enemies
	 * @param Damage - Amount of damage to apply
	 * @return Number of enemies damaged
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Entity Damage")
	static int32 ApplyDamageAtLocation(AActor* WorldContext, FVector HitLocation, float DamageRadius, float Damage);

	/**
	 * Destroy a Mass Entity enemy (removes from system)
	 * Called automatically when an enemy's health reaches 0
	 * @param WorldContext - Any actor in the world (for getting subsystems)
	 * @param TargetNetworkID - The network ID of the entity to destroy
	 * @return True if entity was destroyed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass Entity Damage")
	static bool DestroyMassEntity(AActor* WorldContext, int32 TargetNetworkID);

private:
	static float CalculateTargetScore(
		AActor* SourceActor,
		AActor* Target,
		float AngleToTarget,
		ETargetSelectionMode SelectionMode
	);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterConfigurationAsset.generated.h"

// Forward declarations
class UUserWidget;
class UAnimMontage;
class USkeletalMesh;
class UStaticMesh;
class UAnimInstance;
class UInputMappingContext;
class UInputAction;
class AMageProjectile;

/**
 * Data Asset containing all configuration for a character
 * Replaces hardcoded values and makes characters data-driven
 */
UCLASS(BlueprintType)
class MYPROJECT_API UCharacterConfigurationAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// ========= MOVEMENT CONFIGURATION =========
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 200.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 500.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float JumpVelocity = 700.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	FRotator RotationRate = FRotator(0.f, 400.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControl = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float GroundFriction = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float BrakingDecelerationWalking = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float BrakingDecelerationFalling = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MinAnalogWalkSpeed = 20.f;

	// ========= COMBAT CONFIGURATION =========
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float MeleeDamage = 10.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float MeleeRange = 50.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<class AMageProjectile> ProjectileClass;

	// ========= CAMERA CONFIGURATION =========
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float CameraDistance = 900.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraPitch = -30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	bool bUseCameraLag = false;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (EditCondition = "bUseCameraLag"))
	float CameraLagSpeed = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	bool bUseCameraRotationLag = false;

	// ========= CAPSULE CONFIGURATION =========
	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	float CapsuleRadius = 42.f;

	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	float CapsuleHalfHeight = 96.f;

	// ========= MESH REFERENCES =========
	UPROPERTY(EditDefaultsOnly, Category = "Assets|Mesh")
	TSoftObjectPtr<USkeletalMesh> CharacterMesh;
	
	UPROPERTY(EditDefaultsOnly, Category = "Assets|Mesh")
	FVector MeshRelativeLocation = FVector(0.f, 0.f, -90.f);

	UPROPERTY(EditDefaultsOnly, Category = "Assets|Mesh")
	FRotator MeshRelativeRotation = FRotator(0.f, 270.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Assets|Weapon")
	TSoftObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Assets|Weapon")
	FName WeaponSocketName = TEXT("weapon_r");

	// ========= ANIMATION REFERENCES =========
	UPROPERTY(EditDefaultsOnly, Category = "Assets|Animation")
	TSubclassOf<UAnimInstance> AnimationBlueprint;
	
	// Using a map for flexible animation montage storage
	UPROPERTY(EditDefaultsOnly, Category = "Assets|Animation")
	TMap<FName, TSoftObjectPtr<UAnimMontage>> AnimationMontages;

	// Helper constants for montage names
	static const FName MONTAGE_START_F;
	static const FName MONTAGE_START_R;
	static const FName MONTAGE_ATTACK_1;
	static const FName MONTAGE_ATTACK_2;
	static const FName MONTAGE_DODGE;

	// ========= UI REFERENCES =========
	UPROPERTY(EditDefaultsOnly, Category = "Assets|UI")
	TSubclassOf<UUserWidget> StatsWidgetClass;

	// ========= INPUT REFERENCES =========
	UPROPERTY(EditDefaultsOnly, Category = "Assets|Input")
	TSoftObjectPtr<class UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Assets|Input")
	TSoftObjectPtr<class UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Assets|Input")
	TSoftObjectPtr<class UInputAction> DodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Assets|Input")
	TSoftObjectPtr<class UInputAction> RollAction;

	// ========= PROJECTILE SPAWN CONFIGURATION =========
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile")
	FVector ProjectileSpawnOffset = FVector(100.f, 0.f, 50.f);

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile")
	FRotator ProjectileSpawnRotation = FRotator(0.f, 0.f, 0.f);

	// ========= HELPER METHODS =========
	
	/**
	 * Get a loaded animation montage by name
	 * @param MontageName The name key for the montage
	 * @return The loaded animation montage, or nullptr if not found/loaded
	 */
	UAnimMontage* GetAnimationMontage(FName MontageName) const;

	/**
	 * Validate that all required assets are set
	 * Called in editor to help catch configuration errors early
	 */
	#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

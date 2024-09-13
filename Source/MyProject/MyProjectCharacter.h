// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <random>
#include <cmath>
//#include <numbers>
#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "MyGestureRecognizer.h"
#include "Enums/GestureType.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "./Projectiles/MageProjectile.h"
#include "./MyCharacterMovementComponent.h"
#include "AIController.h"
#include "MyProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

// DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config = Game)
class AMyProjectCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UCameraComponent* FollowCamera;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USceneComponent* CameraRoot;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* RollAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UAnimMontage* DodgeMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UAnimMontage* FirstAttackMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UAnimMontage* SecondAttackMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WeaponMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UArrowComponent* ProjectileSpawnPoint;
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<class AMageProjectile> ProjectileClass;
	/*UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UPoseableMeshComponent* PoseableMesh*/

public:
	AMyProjectCharacter(const FObjectInitializer& ObjectInitializer);
	void InitializeCharacter(bool _isAI, UClass* _AIControllerClass);
	bool GetIsPlayerTryingToMove();
	void SetIsPlayerTryingToMove(bool value);
	void SetAllowPhysicsRotationDuringAnimRootMotion(bool value);
	void SetOrientRotationToMovement(bool value);
	void SetRotationRate(FRotator rotation);
	void SmoothlyRotate(float degrees, float speed);

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	void OnMoving();
	void OnIdle();

	void StartDodge();
	void FinishDodge(UAnimMontage* Montage, bool bInterrupted);

	void StartAttack();
	void FinishAttack(UAnimMontage* Montage, bool bInterrupted);

	void SwitchToWalking();
	void SwitchToRunning();

	void DoNothing(UAnimMontage* Montage, bool bInterrupted);

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

	// Gestures
	void OnSwipeStarted(ETouchIndex::Type FingerIndex, FVector Location);
	void OnSwipeUpdated(ETouchIndex::Type FingerIndex, FVector Location);
	void OnSwipeEnded(ETouchIndex::Type FingerIndex, FVector Location);

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool GetIsWalking();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetInputDirection() const;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool GetIsDodging() const;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetIsInRollAnimation(bool value);
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetLookRotation();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetIsAttackEnding(bool value);
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetIsSecondAttackWindowOpen(bool value);
	void FireProjectile();

private:
	bool				isAI = false;
	bool				IsPlayerTryingToMove = false;
	bool				IsDodging = false;
	bool				IsAttacking = false;
	TArray<FVector>		SwipePoints;
	bool				bIsSwipeInProgress;
	FVector				SwipeStartLocation;
	MyGestureRecognizer Recognizer;
	FVector2D			MovementVector;
	float				StartRollYaw;
	float				RollMovementRotation = 0.f;
	float				StartHorizontalSmoothRotationOffset = 0.f;
	float				EndHorizontalSmoothRotationOffset = 0.f;
	float				CurrentHorizontalSmoothRotationOffset = 0.f;
	float				SmoothRotationSpeed = 1;
	float				SmoothRotationElapsedTime = 0.f;
	FOnMontageEnded		DoNothingDelegate;
	FOnMontageEnded		FinishAttackDelegate;
	float				previusVelocity = 0;

	// Attack animation
	bool IsAttackEnding = false;
	bool IsSecondAttackWindowOpen = false;
};

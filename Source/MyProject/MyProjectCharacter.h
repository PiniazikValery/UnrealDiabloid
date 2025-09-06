// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <random>
#include <cmath>
//#include <numbers>
#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Enums/GestureType.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "./Projectiles/MageProjectile.h"
#include "./MyCharacterMovementComponent.h"
#include "./Character/CharacterInput.h"
#include "AIController.h"
#include "Character/CombatComponent.h"
#include "MyProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UMyGestureRecognizer; // Forward declaration

UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	E_None	 UMETA(DisplayName = "None"),
	E_Melee	 UMETA(DisplayName = "Melee"),
	E_Ranged UMETA(DisplayName = "Ranged"),
	E_Tank	 UMETA(DisplayName = "Tank")
};

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

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UAnimMontage* DodgeMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UAnimMontage* StartFMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UAnimMontage* StartRMontage;
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


public:
	AMyProjectCharacter(const FObjectInitializer& ObjectInitializer);
	void PossessAIController(UClass* _AIControllerClass);
	bool GetIsPlayerTryingToMove();
	void SetIsPlayerTryingToMove(bool value);
	void SetAllowPhysicsRotationDuringAnimRootMotion(bool value);
	void SetOrientRotationToMovement(bool value);
	void SetRotationRate(FRotator rotation);
	void SmoothlyRotate(float degrees, float speed);
	void SwitchToWalking();
	void SwitchToRunning();
	void SetMovementVector(FVector2D _MovementVector);
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character")
	bool IsPlayerTryingToMove = false;
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetIsPlayerTryingToMove(bool NewIsPlayerTryingToMove);


protected:
	void StartDodge();
	void FinishDodge(UAnimMontage* Montage, bool bInterrupted);

	void StartAttack();
	void FinishAttack(UAnimMontage* Montage, bool bInterrupted);

	void DoNothing(UAnimMontage* Montage, bool bInterrupted);

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Gestures
	void OnSwipeStarted(ETouchIndex::Type FingerIndex, FVector Location);
	void OnSwipeUpdated(ETouchIndex::Type FingerIndex, FVector Location);
	void OnSwipeEnded(ETouchIndex::Type FingerIndex, FVector Location);
	// Server RPC for playing montage
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartDodge();

	// Multicast RPC to play montage on all clients
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartDodge();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartAttack(float angle);

	// Multicast RPC to play montage on all clients
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartAttack(float angle);

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool GetWithoutRootStart();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool GetIsWalking();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetInputDirection() const;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool GetIsDodging() const;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool GetIsAttacking() const;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetIsInRollAnimation(bool value);
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetLookRotation();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetIsAttackEnding(bool value);
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetIsSecondAttackWindowOpen(bool value);
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetSecondAttackWindow(bool bOpen);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterInput* InputHandler;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UMyGestureRecognizer* GestureRecognizer;
	void FireProjectile();

private:
	/*float				PreviousSpeed;*/
	bool				withoutRootStart = false;
	TArray<FVector>		SwipePoints;
	bool				bIsSwipeInProgress;
	FVector				SwipeStartLocation;
	FVector2D			MovementVector;
	float				StartRollYaw;
	float				RollMovementRotation = 0.f;
	float				StartHorizontalSmoothRotationOffset = 0.f;
	float				EndHorizontalSmoothRotationOffset = 0.f;
	float				CurrentHorizontalSmoothRotationOffset = 0.f;
	float				SmoothRotationSpeed = 1;
	float				SmoothRotationElapsedTime = 0.f;
	FOnMontageEnded		DoNothingDelegate;
	float				previusVelocity = 0;
	UInputComponent*	_PlayerInputComponent;
   	void InitializeMesh();
	void InitializeWeapon();
	void InitializeAnimations();
	void InitializeInput();
	void InitializeMovement();
	void InitializeCamera();
	void InitializeProjectileSpawnPoint();
	void PlayMontage(UAnimMontage* Montage, FOnMontageEnded EndDelegate);
	UFUNCTION()
	void HandleGesture(EGestureType Gesture);

public:
	FORCEINLINE UAnimMontage* GetDodgeMontage() const { return DodgeMontage; }
	FORCEINLINE UAnimMontage* GetFirstAttackMontage() const { return FirstAttackMontage; }
	FORCEINLINE UAnimMontage* GetSecondAttackMontage() const { return SecondAttackMontage; }
	FORCEINLINE UCombatComponent* GetCombatComponent() const { return CombatComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	UCombatComponent* CombatComponent;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Enums/GestureType.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "Projectiles/MageProjectile.h"
#include "./MyCharacterMovementComponent.h"
#include "./Character/CharacterInput.h"
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	class URotationSmoothingComponent* RotationSmoothingComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	class UProjectileSpawnerComponent* ProjectileSpawnerComponent;


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
	// Combat actions handled directly through CombatComponent in multicast RPCs.

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
	bool				withoutRootStart = false;
	FVector2D			MovementVector;
	// rotation smoothing moved to RotationSmoothingComponent
	UInputComponent*	_PlayerInputComponent = nullptr;
	void InitializeMesh();
	void InitializeWeapon();
	void InitializeAnimations();
	void InitializeInput();
	void InitializeMovement();
	void InitializeCamera();
	void InitializeProjectileSpawnPoint();
	UFUNCTION()
	void HandleGesture(EGestureType Gesture);
	UFUNCTION()
	void HandleRotationOffsetChanged(float NewOffset);

public:
	FORCEINLINE UAnimMontage* GetDodgeMontage() const { return DodgeMontage; }
	FORCEINLINE UAnimMontage* GetFirstAttackMontage() const { return FirstAttackMontage; }
	FORCEINLINE UAnimMontage* GetSecondAttackMontage() const { return SecondAttackMontage; }
	FORCEINLINE UCombatComponent* GetCombatComponent() const { return CombatComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	UCombatComponent* CombatComponent;
};

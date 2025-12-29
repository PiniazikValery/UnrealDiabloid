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
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "CharacterConfigurationAsset.h"
#include "AutoAimHelper.h"
#include "MyProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UMyGestureRecognizer; // Forward declaration
class UCharacterSetupComponent; // Forward declaration
class UCharacterNetworkComponent; // Forward declaration

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

public:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	class USceneComponent* CameraRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	UStaticMeshComponent* WeaponMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	UArrowComponent* ProjectileSpawnPoint;

	// Setup Component - handles all initialization
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterSetupComponent* SetupComponent;

	/** Network Component - handles all RPC and replication logic */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterNetworkComponent* NetworkComponent;

private:

	// ========= CONFIGURATION ASSET =========
	/** Configuration asset containing all character settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
	UCharacterConfigurationAsset* CharacterConfig;

	// ========= CACHED REFERENCES (loaded from config) =========
	/** MappingContext - loaded from config */
	UPROPERTY(BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action - loaded from config */
	UPROPERTY(BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;
	
	UPROPERTY(BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* RollAction;
	
	/** Dodge Input Action - loaded from config */
	UPROPERTY(BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DodgeAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	
	// DEPRECATED: Animation montages - Use AnimationComponent instead
	// Kept temporarily for backward compatibility
	UPROPERTY(BlueprintReadOnly, Category = Animation, meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AnimationComponent->GetFirstAttackMontage() instead"))
	UAnimMontage* StartFMontage;
	UPROPERTY(BlueprintReadOnly, Category = Animation, meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AnimationComponent instead"))
	UAnimMontage* StartRMontage;
	UPROPERTY(BlueprintReadOnly, Category = Animation, meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AnimationComponent->GetFirstAttackMontage() instead"))
	UAnimMontage* FirstAttackMontage;
	UPROPERTY(BlueprintReadOnly, Category = Animation, meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AnimationComponent->GetSecondAttackMontage() instead"))
	UAnimMontage* SecondAttackMontage;
	
	// Projectile class - loaded from config (kept as property for backward compatibility)
	TSubclassOf<class AMageProjectile> ProjectileClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	class URotationSmoothingComponent* RotationSmoothingComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	class UProjectileSpawnerComponent* ProjectileSpawnerComponent;


public:
	AMyProjectCharacter(const FObjectInitializer& ObjectInitializer);
	void PossessAIController(UClass* _AIControllerClass);
	
	// Network component wrapper functions for backward compatibility
	UFUNCTION(BlueprintCallable, Category = "Character|Network")
	bool GetIsPlayerTryingToMove() const;
	
	UFUNCTION(BlueprintCallable, Category = "Character|Network")
	void SetIsPlayerTryingToMove(bool bValue);
	
	void SetAllowPhysicsRotationDuringAnimRootMotion(bool value);
	void SetOrientRotationToMovement(bool value);
	void SetRotationRate(FRotator rotation);
	void SmoothlyRotate(float degrees, float speed);
	void SwitchToWalking();
	void SwitchToRunning();
	void SetMovementVector(FVector2D _MovementVector);

	// Damage funnel to stats component
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;


protected:
	// Combat actions handled directly through CombatComponent in multicast RPCs.

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
	virtual void PostInitializeComponents() override;

	// Gestures
	void OnSwipeStarted(ETouchIndex::Type FingerIndex, FVector Location);
	void OnSwipeUpdated(ETouchIndex::Type FingerIndex, FVector Location);
	void OnSwipeEnded(ETouchIndex::Type FingerIndex, FVector Location);
	// Enhanced Input Action handlers
	void OnRoll();
	void OnDodge();
	// Dodge now fully handled inside CombatComponent with prediction; wrappers removed.

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
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetIsAttacking(bool value);
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void DetectHit();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterInput* InputHandler;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UMyGestureRecognizer* GestureRecognizer;
	
	/** NEW: Animation component - handles all animation logic */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UCharacterAnimationComponent* AnimationComponent;
	void FireProjectile();

	// UI - loaded from config
	TSubclassOf<UUserWidget> CharacterStatsWidgetClass;
	UPROPERTY()
	UUserWidget* CharacterStatsWidget = nullptr;

private:
	bool				withoutRootStart = false;
	FVector2D			MovementVector;
	// rotation smoothing moved to RotationSmoothingComponent
	UInputComponent*	_PlayerInputComponent = nullptr;
	FTimerHandle		InputSetupRetryTimer;
	int32				InputSetupRetryCount = 0;

	// Smooth speed interpolation to prevent network teleporting
	// Both client and server interpolate to target, reducing position errors
	float TargetMaxWalkSpeed = 0.f;
	static constexpr float SpeedInterpRate = 15.f; // How fast to interpolate (higher = faster)

	void InitializeMesh();
	void InitializeInput();
	void RetryInputSetup();
	void UpdateSpeedInterpolation(float DeltaTime);
	UFUNCTION()
	void HandleGesture(EGestureType Gesture);
	UFUNCTION()
	void HandleRotationOffsetChanged(float NewOffset);
	// Animation event handlers
	UFUNCTION()
	void HandleAnimationComplete(FName AnimationName);
	UFUNCTION()
	void HandleAnimationStarted(FName AnimationName);

protected:
	// Auto-aim settings
	UPROPERTY(EditAnywhere, Category = "Combat|Auto-Aim")
	float AutoAimRange = 5000.f;
	
	UPROPERTY(EditAnywhere, Category = "Combat|Auto-Aim")
	float AutoAimMaxAngle = 90.f;
	
	UPROPERTY(EditAnywhere, Category = "Combat|Auto-Aim")
	bool bEnableAutoAim = true;
	
	UPROPERTY(EditAnywhere, Category = "Combat|Auto-Aim")
	ETargetSelectionMode AutoAimMode = ETargetSelectionMode::ClosestToCenter;

	// Current target Mass Entity NetworkID (set by auto-aim, used by projectiles)
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Auto-Aim")
	int32 CurrentTargetMassEntityNetworkID = INDEX_NONE;

	// Stats death handler - protected so derived classes can override
	UFUNCTION()
	virtual void HandleDeath();

public: // ========= Stats Component =========
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class UCharacterStatsComponent* StatsComponent;
	UFUNCTION(BlueprintPure, Category="Stats") FORCEINLINE UCharacterStatsComponent* GetStatsComponent() const { return StatsComponent; }
	// Legacy convenience wrappers (redirect to StatsComponent)
	UFUNCTION(BlueprintCallable, Category="Stats") bool SpendMana(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats") void RestoreMana(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats") void Heal(float Amount);
	UFUNCTION(BlueprintPure, Category="Stats") bool IsAlive() const;

public:
	FORCEINLINE UAnimMontage* GetFirstAttackMontage() const { return FirstAttackMontage; }
	FORCEINLINE UAnimMontage* GetSecondAttackMontage() const { return SecondAttackMontage; }
	FORCEINLINE UCombatComponent* GetCombatComponent() const { return CombatComponent; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UCombatComponent* CombatComponent;
};

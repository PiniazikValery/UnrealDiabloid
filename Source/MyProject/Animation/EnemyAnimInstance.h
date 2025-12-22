// EnemyAnimInstance.h
// Animation instance for Mass-controlled enemy skeletal meshes
// Mirrors player animation variables for consistent Animation Blueprint logic

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

// Movement input direction enum - matches your player's EMovementInput
UENUM(BlueprintType)
enum class EEnemyMovementInput : uint8
{
	Forward,
	ForwardRight,
	Right,
	BackwardRight,
	Backward,
	BackwardLeft,
	Left,
	ForwardLeft
};

// Animation state for enemies
UENUM(BlueprintType)
enum class EEnemyAnimationState : uint8
{
	Idle,
	Locomotion,
	Attack,
	Hit,
	Death,
	Stunned,
	Special
};

/**
 * Animation Instance for Mass Entity enemies
 * Values are set directly by the EnemyVisualizationProcessor - no pawn owner needed
 * 
 * Use the same Animation Blueprint logic as your player by reading these properties
 */
UCLASS()
class MYPROJECT_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UEnemyAnimInstance();

	// ========================================================================
	// MOVEMENT PROPERTIES (Mirrors your player's MyAnimInstance)
	// ========================================================================

	// Current velocity vector
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	FVector Velocity = FVector::ZeroVector;

	// Speed on ground (Velocity.Size())
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	float GroundSpeed = 0.0f;

	// Movement direction relative to facing (-180 to 180)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	float Direction = 0.0f;

	// Max possible speed (for normalization in blendspaces)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	float MaxSpeed = 600.0f;

	// Has any velocity
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	bool HasVelocity = false;

	// Should be moving (has acceleration and speed > threshold)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	bool ShouldMove = false;

	// Currently falling
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	bool IsFalling = false;

	// Has acceleration input
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	bool HasAcceleration = false;

	// Is actively accelerating
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	bool IsAccelerating = false;

	// Discrete movement input direction
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	EEnemyMovementInput MovementInput = EEnemyMovementInput::Forward;

	// Distance traveled since last frame
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Movement")
	float DistanceTraveled = 0.0f;

	// ========================================================================
	// MOMENTUM PROPERTIES (Mirrors your player's momentum settings)
	// ========================================================================

	// Last update velocity from movement component
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Momentum")
	FVector LastUpdateVelocity = FVector::ZeroVector;

	// Whether to use separate braking friction
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Momentum")
	bool UseSeparateBrakingFriction = false;

	// Braking friction value
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Momentum")
	float BrakingFriction = 0.0f;

	// Ground friction value
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Momentum")
	float GroundFriction = 8.0f;

	// Braking friction factor
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Momentum")
	float BrakingFrictionFactor = 2.0f;

	// Braking deceleration when walking
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Momentum")
	float BrakingDecelerationWalking = 2048.0f;

	// ========================================================================
	// COMBAT/STATE PROPERTIES
	// ========================================================================

	// Current high-level animation state
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|State")
	EEnemyAnimationState AnimationState = EEnemyAnimationState::Idle;

	// Is currently attacking
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	bool IsAttacking = false;

	// Was hit (trigger for hit reaction)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	bool WasHit = false;

	// Is dead
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	bool IsDead = false;

	// Is stunned/staggered
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	bool IsStunned = false;

	// Attack type index (for different attack animations)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	int32 AttackType = 0;

	// Hit direction for directional hit reactions (-180 to 180)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	float HitDirection = 0.0f;

	// ========================================================================
	// ROTATION/FACING
	// ========================================================================

	// Target look rotation (for aim offsets or head turning)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Rotation")
	FRotator LookRotation = FRotator::ZeroRotator;

	// Yaw offset for leaning/turning animations
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Rotation")
	float YawOffset = 0.0f;

	// ========================================================================
	// ANIMATION PLAYBACK
	// ========================================================================

	// Global play rate multiplier
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Playback")
	float PlayRateMultiplier = 1.0f;

	// Normalized time for synced animations (0-1)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Playback")
	float NormalizedAnimTime = 0.0f;

	// ========================================================================
	// MONTAGES (Set in Blueprint defaults)
	// ========================================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Montages")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Montages")
	TObjectPtr<UAnimMontage> HitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Montages")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Montages")
	TObjectPtr<UAnimMontage> StunnedMontage;

	// ========================================================================
	// PUBLIC API - Called by Visualization Processor
	// ========================================================================

	/**
	 * Main update function called by the processor each frame
	 * Sets all movement-related properties at once for efficiency
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void UpdateMovement(
		const FVector& InVelocity,
		const FVector& InAcceleration,
		float InMaxSpeed,
		bool bInIsFalling,
		const FVector& InFacingDirection);

	/**
	 * Set momentum properties (mirrors player's SetMomentumProperties)
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void SetMomentumProperties(
		const FVector& InLastUpdateVelocity,
		bool bInUseSeparateBrakingFriction,
		float InBrakingFriction,
		float InGroundFriction,
		float InBrakingFrictionFactor,
		float InBrakingDecelerationWalking);

	/**
	 * Set combat state
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void SetCombatState(EEnemyAnimationState NewState, bool bInIsAttacking = false, int32 InAttackType = 0);

	/**
	 * Trigger hit reaction
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void TriggerHitReaction(float InHitDirection);

	/**
	 * Trigger death
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void TriggerDeath();

	/**
	 * Set look/aim target
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void SetLookRotation(const FRotator& InLookRotation);

	/**
	 * Reset to idle state (when returning to pool)
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Update")
	void ResetToIdle();

	/**
	 * Play attack montage by index
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Montage")
	float PlayAttackMontage(int32 AttackIndex, float PlayRate = 1.0f);

	// ========================================================================
	// BLUEPRINT EVENTS
	// ========================================================================

	// Called when animation state changes
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Events")
	void OnAnimationStateChanged(EEnemyAnimationState OldState, EEnemyAnimationState NewState);

	// Called when hit reaction triggers
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Events")
	void OnHitReaction(float InHitDirection);

	// Called on death
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Events")
	void OnDeath();

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ========================================================================
	// INTERNAL HELPERS
	// ========================================================================

	/** Calculate Direction from velocity and facing - same logic as your player */
	void CalculateDirection(const FVector& InVelocity, const FVector& InFacingDirection);

	/** Set MovementInput enum from Direction - same logic as your player */
	void SetMovementInputFromDirection();

private:
	// Previous location for distance calculation
	FVector PreviousLocation = FVector::ZeroVector;
	bool bFirstUpdate = true;

	// For smoothing HasAcceleration like your player does
	float AccelerationSmoothTimer = 0.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Tuning")
	float AccelerationSmoothDelay = 0.1f;
};
// EnemyAnimInstance.cpp
// Implementation of enemy animation instance for Mass entities

#include "EnemyAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UEnemyAnimInstance::UEnemyAnimInstance()
{
	// Movement properties
	Velocity = FVector::ZeroVector;
	GroundSpeed = 0.0f;
	Direction = 0.0f;
	MaxSpeed = 600.0f;
	HasVelocity = false;
	ShouldMove = false;
	IsFalling = false;
	HasAcceleration = false;
	IsAccelerating = false;
	MovementInput = EEnemyMovementInput::Forward;
	DistanceTraveled = 0.0f;
	
	// Momentum properties
	LastUpdateVelocity = FVector::ZeroVector;
	UseSeparateBrakingFriction = false;
	BrakingFriction = 0.0f;
	GroundFriction = 8.0f;
	BrakingFrictionFactor = 2.0f;
	BrakingDecelerationWalking = 2048.0f;
	
	// Combat/state properties
	AnimationState = EEnemyAnimationState::Idle;
	IsAttacking = false;
	WasHit = false;
	IsDead = false;
	IsStunned = false;
	AttackType = 0;
	HitDirection = 0.0f;
	
	// Rotation
	LookRotation = FRotator::ZeroRotator;
	YawOffset = 0.0f;
	
	// Playback
	PlayRateMultiplier = 1.0f;
	NormalizedAnimTime = 0.0f;
	
	// Internal
	AccelerationSmoothDelay = 0.1f;
	AccelerationSmoothTimer = 0.0f;
	bFirstUpdate = true;
}

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	PreviousLocation = FVector::ZeroVector;
	bFirstUpdate = true;
	AccelerationSmoothTimer = 0.0f;
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	// Clear one-shot triggers
	if (WasHit)
	{
		WasHit = false;
	}
	
	// Update normalized animation time (wraps 0-1)
	NormalizedAnimTime = FMath::Fmod(NormalizedAnimTime + DeltaSeconds, 1.0f);
}

void UEnemyAnimInstance::UpdateMovement(
	const FVector& InVelocity,
	const FVector& InAcceleration,
	float InMaxSpeed,
	bool bInIsFalling,
	const FVector& InFacingDirection)
{
	// Store velocity
	Velocity = InVelocity;
	GroundSpeed = Velocity.Size();
	HasVelocity = !Velocity.IsNearlyZero();
	MaxSpeed = InMaxSpeed;
	IsFalling = bInIsFalling;
	
	// Calculate direction relative to facing (same as player's CalculateDirection)
	CalculateDirection(InVelocity, InFacingDirection);
	
	// Set discrete movement input
	SetMovementInputFromDirection();
	
	// Handle acceleration with smoothing (same as player's logic)
	bool bHasRawAcceleration = !InAcceleration.IsNearlyZero();
	
	if (bHasRawAcceleration)
	{
		HasAcceleration = true;
		AccelerationSmoothTimer = AccelerationSmoothDelay;
	}
	else
	{
		if (AccelerationSmoothTimer > 0.0f)
		{
			float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
			AccelerationSmoothTimer -= DeltaTime;
			HasAcceleration = true;
		}
		else
		{
			HasAcceleration = false;
		}
	}
	
	IsAccelerating = InAcceleration.SizeSquared() > 1.0f;
	ShouldMove = HasAcceleration && GroundSpeed > 3.0f;
	
	// Calculate distance traveled
	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	if (MeshComp)
	{
		FVector CurrentLocation = MeshComp->GetComponentLocation();
		
		if (bFirstUpdate)
		{
			PreviousLocation = CurrentLocation;
			bFirstUpdate = false;
			DistanceTraveled = 0.0f;
		}
		else
		{
			DistanceTraveled = FVector::Dist(CurrentLocation, PreviousLocation);
			PreviousLocation = CurrentLocation;
		}
	}
}

void UEnemyAnimInstance::SetMomentumProperties(
	const FVector& InLastUpdateVelocity,
	bool bInUseSeparateBrakingFriction,
	float InBrakingFriction,
	float InGroundFriction,
	float InBrakingFrictionFactor,
	float InBrakingDecelerationWalking)
{
	LastUpdateVelocity = InLastUpdateVelocity;
	UseSeparateBrakingFriction = bInUseSeparateBrakingFriction;
	BrakingFriction = InBrakingFriction;
	GroundFriction = InGroundFriction;
	BrakingFrictionFactor = InBrakingFrictionFactor;
	BrakingDecelerationWalking = InBrakingDecelerationWalking;
}

void UEnemyAnimInstance::CalculateDirection(const FVector& InVelocity, const FVector& InFacingDirection)
{
	// Same logic as player's CalculateDirection()
	if (InVelocity.IsNearlyZero() || InFacingDirection.IsNearlyZero())
	{
		Direction = 0.0f;
		return;
	}
	
	FVector Forward = InFacingDirection.GetSafeNormal();
	FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	
	FVector VelocityNorm = InVelocity.GetSafeNormal();
	
	float ForwardSpeed = FVector::DotProduct(VelocityNorm, Forward);
	float RightSpeed = FVector::DotProduct(VelocityNorm, Right);
	
	Direction = FMath::Atan2(RightSpeed, ForwardSpeed);
	Direction = FMath::RadiansToDegrees(Direction);
}

void UEnemyAnimInstance::SetMovementInputFromDirection()
{
	// Exact same logic as player's SetMovementInput()
	if (Direction > -22.5f && Direction < 22.5f)
	{
		MovementInput = EEnemyMovementInput::Forward;
	}
	else if (Direction >= 22.5f && Direction <= 67.5f)
	{
		MovementInput = EEnemyMovementInput::ForwardRight;
	}
	else if (Direction > 67.5f && Direction < 112.5f)
	{
		MovementInput = EEnemyMovementInput::Right;
	}
	else if (Direction >= 112.5f && Direction <= 157.5f)
	{
		MovementInput = EEnemyMovementInput::BackwardRight;
	}
	else if (Direction > 157.5f || Direction < -157.5f)
	{
		MovementInput = EEnemyMovementInput::Backward;
	}
	else if (Direction >= -157.5f && Direction <= -112.5f)
	{
		MovementInput = EEnemyMovementInput::BackwardLeft;
	}
	else if (Direction > -112.5f && Direction < -67.5f)
	{
		MovementInput = EEnemyMovementInput::Left;
	}
	else if (Direction >= -67.5f && Direction <= -22.5f)
	{
		MovementInput = EEnemyMovementInput::ForwardLeft;
	}
}

void UEnemyAnimInstance::SetCombatState(EEnemyAnimationState NewState, bool bInIsAttacking, int32 InAttackType)
{
	EEnemyAnimationState OldState = AnimationState;
	
	AnimationState = NewState;
	IsAttacking = bInIsAttacking;
	AttackType = InAttackType;
	
	if (OldState != NewState)
	{
		OnAnimationStateChanged(OldState, NewState);
	}
}

void UEnemyAnimInstance::TriggerHitReaction(float InHitDirection)
{
	if (IsDead)
	{
		return; // Don't play hit reactions if dead
	}
	
	WasHit = true;
	HitDirection = InHitDirection;
	AnimationState = EEnemyAnimationState::Hit;
	
	// Play hit montage if set
	if (HitReactionMontage)
	{
		Montage_Play(HitReactionMontage, PlayRateMultiplier);
	}
	
	OnHitReaction(InHitDirection);
}

void UEnemyAnimInstance::TriggerDeath()
{
	if (IsDead)
	{
		return; // Already dead
	}
	
	IsDead = true;
	AnimationState = EEnemyAnimationState::Death;
	IsAttacking = false;
	
	// Play death montage if set
	if (DeathMontage)
	{
		Montage_Play(DeathMontage, PlayRateMultiplier);
	}
	
	OnDeath();
}

void UEnemyAnimInstance::SetLookRotation(const FRotator& InLookRotation)
{
	LookRotation = InLookRotation;
	
	// Calculate yaw offset from current facing
	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	if (MeshComp)
	{
		FRotator MeshRotation = MeshComp->GetComponentRotation();
		YawOffset = FMath::UnwindDegrees(InLookRotation.Yaw - MeshRotation.Yaw);
		YawOffset = FMath::Clamp(YawOffset, -90.0f, 90.0f);
	}
}

void UEnemyAnimInstance::ResetToIdle()
{
	// Reset movement
	Velocity = FVector::ZeroVector;
	GroundSpeed = 0.0f;
	Direction = 0.0f;
	HasVelocity = false;
	ShouldMove = false;
	IsFalling = false;
	HasAcceleration = false;
	IsAccelerating = false;
	MovementInput = EEnemyMovementInput::Forward;
	DistanceTraveled = 0.0f;
	
	// Reset momentum
	LastUpdateVelocity = FVector::ZeroVector;
	
	// Reset combat/state
	AnimationState = EEnemyAnimationState::Idle;
	IsAttacking = false;
	WasHit = false;
	IsDead = false;
	IsStunned = false;
	AttackType = 0;
	HitDirection = 0.0f;
	
	// Reset rotation
	LookRotation = FRotator::ZeroRotator;
	YawOffset = 0.0f;
	
	// Reset playback
	PlayRateMultiplier = 1.0f;
	NormalizedAnimTime = 0.0f;
	
	// Reset internal state
	AccelerationSmoothTimer = 0.0f;
	bFirstUpdate = true;
	
	// Stop any playing montages
	Montage_Stop(0.2f);
}

float UEnemyAnimInstance::PlayAttackMontage(int32 AttackIndex, float PlayRate)
{
	if (!AttackMontages.IsValidIndex(AttackIndex))
	{
		return 0.0f;
	}
	
	UAnimMontage* Montage = AttackMontages[AttackIndex];
	if (!Montage)
	{
		return 0.0f;
	}
	
	IsAttacking = true;
	AttackType = AttackIndex;
	AnimationState = EEnemyAnimationState::Attack;
	
	float MontageLength = Montage_Play(Montage, PlayRate * PlayRateMultiplier);
	
	return MontageLength;
}
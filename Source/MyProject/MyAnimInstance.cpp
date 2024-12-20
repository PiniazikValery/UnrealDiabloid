// Fill out your copyright notice in the Description page of Project Settings.

#include "MyAnimInstance.h"

void UMyAnimInstance::NativeInitializeAnimation()
{
	StartYaw = 0;
	PreviousLocation = FVector::ZeroVector;
	DistanceTraveled = 0.0f;
	CharacterReference = Cast<AMyProjectCharacter>(TryGetPawnOwner());
	if (CharacterReference)
	{
		CharacterMovementReference = CharacterReference->GetCharacterMovement();
	}
	Super::NativeInitializeAnimation();
}

void UMyAnimInstance::CalculateDirection()
{
	FVector Forward = CharacterReference->GetActorForwardVector().GetSafeNormal();
	FVector Right = CharacterReference->GetActorRightVector().GetSafeNormal();
	float	ForwardSpeed = FVector::DotProduct(Velocity.GetSafeNormal(), Forward);
	float	RightSpeed = FVector::DotProduct(Velocity.GetSafeNormal(), Right);
	Direction = FMath::Atan2(RightSpeed, ForwardSpeed);
	Direction = FMath::RadiansToDegrees(Direction);
}

void UMyAnimInstance::SetDodgeProperties()
{
	PreviusIsDodging = IsDodging;
	IsDodging = CharacterReference->GetIsDodging();
}

void UMyAnimInstance::SetMomentumProperties()
{
	LastUpdateVelocity = CharacterMovementReference->GetLastUpdateVelocity();
	UseSeparateBrakingFriction = CharacterMovementReference->bUseSeparateBrakingFriction;
	BrakingFriction = CharacterMovementReference->BrakingFriction;
	GroundFriction = CharacterMovementReference->GroundFriction;
	BrakingFrictionFactor = CharacterMovementReference->BrakingFrictionFactor;
	BrakingDecelerationWalking = CharacterMovementReference->BrakingDecelerationWalking;
}

void UMyAnimInstance::SetMovementProperties()
{
	LookRotation = CharacterReference->GetLookRotation();
	IsWalking = CharacterReference->GetIsWalking();
	IsWithoutRootStart = CharacterReference->GetWithoutRootStart();
	IsAttacking = CharacterReference->GetIsAttacking();
	IsPlayerTryingToMove = CharacterReference->GetIsPlayerTryingToMove();
	float NextInputDirection = FMath::UnwindDegrees(CharacterReference->GetInputDirection() - StartYaw);
	if (!IsPlayerTryingToMove || FMath::Abs(InputDirection - NextInputDirection) >= 45)
	{
		StartYaw = CharacterReference->GetActorRotation().Yaw;
		NextInputDirection = FMath::UnwindDegrees(CharacterReference->GetInputDirection() - StartYaw);
	}
	InputDirection = NextInputDirection;
	Velocity = CharacterReference->GetVelocity();
	GroundSpeed = Velocity.Size();
	ShouldMove = !CharacterMovementReference->GetCurrentAcceleration().IsZero() && GroundSpeed > 3;
	IsFalling = CharacterMovementReference->IsFalling();
	IsAccelerating = CharacterMovementReference->GetCurrentAcceleration().SizeSquared() > 1;
}

void UMyAnimInstance::SetMovementInput()
{
	if (Direction > -22.5 && Direction < 22.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("Forward"));
		MovementInput = EMovementInput::Forward;
	}
	else if (Direction >= 22.5 && Direction <= 67.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("ForwardRight"));
		MovementInput = EMovementInput::ForwardRight;
	}
	else if (Direction > 67.5 && Direction < 112.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("Right"));
		MovementInput = EMovementInput::Right;
	}
	else if (Direction >= 112.5 && Direction <= 157.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("BackwardRight"));
		MovementInput = EMovementInput::BackwardRight;
	}
	else if (Direction > 157.5 || Direction < -157.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("Backward"));
		MovementInput = EMovementInput::Backward;
	}
	else if (Direction >= -157.5 && Direction <= -112.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("BackwardLeft"));
		MovementInput = EMovementInput::BackwardLeft;
	}
	else if (Direction > -112.5 && Direction < -67.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("Left"));
		MovementInput = EMovementInput::Left;
	}
	else if (Direction >= -67.5 && Direction <= -22.5)
	{
		UE_LOG(LogTemp, Warning, TEXT("ForwardLeft"));
		MovementInput = EMovementInput::ForwardLeft;
	}
}

void UMyAnimInstance::CalculateDistanceTraveled()
{
	FVector CurrentLocation = CharacterReference->GetActorLocation();
	DistanceTraveled = FVector::Distance(CurrentLocation, PreviousLocation);
	PreviousLocation = CurrentLocation;
}

float UMyAnimInstance::Unwind(float value) const
{
	return FMath::UnwindDegrees(value);
}

void UMyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	if (CharacterReference && CharacterMovementReference)
	{
		SetDodgeProperties();
		SetMovementProperties();
		CalculateDirection();
		SetMomentumProperties();
		SetMovementInput();
		CalculateDistanceTraveled();
	}
}

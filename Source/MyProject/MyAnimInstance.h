// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enums/MovementInput.h"
#include "Animation/AnimInstance.h"
#include "MyProjectCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyAnimInstance.generated.h"

/**
 *
 */
UCLASS()
class MYPROJECT_API UMyAnimInstance : public UAnimInstance
{

	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe), BlueprintPure, Category = "Animation")
	float Unwind(float value) const;

protected:
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeInitializeAnimation() override;

	class AMyProjectCharacter*		   CharacterReference;
	class UCharacterMovementComponent* CharacterMovementReference;

	FVector PreviousLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	FVector Velocity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float GroundSpeed;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool ShouldMove;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool IsFalling;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool IsAccelerating;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float Direction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float InputDirection;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float StartYaw;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	EMovementInput MovementInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float DistanceTraveled;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	FVector LastUpdateVelocity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool UseSeparateBrakingFriction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float BrakingFriction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float GroundFriction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float BrakingFrictionFactor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float BrakingDecelerationWalking;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool IsPlayerTryingToMove;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool IsDodging = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool PreviusIsDodging = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	bool IsWalking = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation Properties")
	float LookRotation = 0.f;

private:
	void CalculateDistanceTraveled();
	void CalculateDirection();
	void SetDodgeProperties();
	void SetMomentumProperties();
	void SetMovementProperties();
	void SetMovementInput();
};

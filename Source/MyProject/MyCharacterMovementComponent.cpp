// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacterMovementComponent.h"

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	bOrientRotationToMovement = false;
	bUseAccelerationForPaths = true;
	bUseControllerDesiredRotation = true;
	BrakingDecelerationWalking = 512.0f;
	RotationRate = FRotator(0.0f, 0.0f, 0.0f);
}

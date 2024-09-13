// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacterMovementComponent.h"

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	bUseAccelerationForPaths = true;
	/*bUseRVOAvoidance = true;
	AvoidanceConsiderationRadius = 150.0f;
	AvoidanceWeight = 0.2f;*/
}

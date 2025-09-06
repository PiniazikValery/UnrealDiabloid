// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h" // Ensure base class definition is available
#include "Enums/GestureType.h"
#include "UMyGestureRecognizer.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMyGestureRecognizer: public UActorComponent
{
	GENERATED_BODY()
public:
	UMyGestureRecognizer();
	EGestureType RecognizeGesture(const TArray<FVector>& InPoints);
	void StartGesture(const FVector& Start);
	void UpdateGesture(const FVector& Point);
	void EndGesture(const FVector& End);

	UPROPERTY(BlueprintAssignable)
	FOnGestureRecognized OnGestureRecognized;

private:
	TArray<FVector> LocalPoints;
	FVector StartLocation;
	bool bGestureActive = false;
	const float SwipeThreshold = 100.0f;
	bool	IsCircleGesture(const TArray<FVector>& InPoints);
	FVector CalculateCenter(const TArray<FVector>& InPoints);
	float	CalculateAverageRadius(const TArray<FVector>& InPoints, const FVector& Center);
	float	CalculateRadiusVariance(const TArray<FVector>& InPoints, const FVector& Center, float AverageRadius);
};

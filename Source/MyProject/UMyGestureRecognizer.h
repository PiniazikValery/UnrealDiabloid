// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enums/GestureType.h"
#include "UMyGestureRecognizer.generated.h"

/**
 *
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMyGestureRecognizer: public UActorComponent
{
	GENERATED_BODY()
public:
	UMyGestureRecognizer();
	~UMyGestureRecognizer();
	EGestureType RecognizeGesture(const TArray<FVector>& Points);
	void StartGesture(const FVector& Start);
	void UpdateGesture(const FVector& Point);
	void EndGesture(const FVector& End);

	UPROPERTY(BlueprintAssignable)
	FOnGestureRecognized OnGestureRecognized;

private:
	TArray<FVector> Points;
	FVector StartLocation;
	bool bGestureActive = false;
	const float SwipeThreshold = 100.0f; // Adjust this value based on your needs
	bool	IsCircleGesture(const TArray<FVector>& Points);
	FVector CalculateCenter(const TArray<FVector>& Points);
	float	CalculateAverageRadius(const TArray<FVector>& Points, const FVector& Center);
	float	CalculateRadiusVariance(const TArray<FVector>& Points, const FVector& Center, float AverageRadius);
};

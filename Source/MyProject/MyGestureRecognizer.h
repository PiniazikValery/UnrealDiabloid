// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enums/GestureType.h"

/**
 *
 */
class MYPROJECT_API MyGestureRecognizer
{
public:
	MyGestureRecognizer();
	~MyGestureRecognizer();
	EGestureType RecognizeGesture(const TArray<FVector>& Points);

private:
	const float SwipeThreshold = 100.0f; // Adjust this value based on your needs

	bool	IsCircleGesture(const TArray<FVector>& Points);
	FVector CalculateCenter(const TArray<FVector>& Points);
	float	CalculateAverageRadius(const TArray<FVector>& Points, const FVector& Center);
	float	CalculateRadiusVariance(const TArray<FVector>& Points, const FVector& Center, float AverageRadius);
};

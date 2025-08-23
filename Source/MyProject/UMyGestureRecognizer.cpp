// Fill out your copyright notice in the Description page of Project Settings.

#include "UMyGestureRecognizer.h"

UMyGestureRecognizer::UMyGestureRecognizer()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UMyGestureRecognizer::~UMyGestureRecognizer()
{
}

void UMyGestureRecognizer::StartGesture(const FVector& Start)
{
	StartLocation = Start;
	Points.Empty();
	Points.Add(Start);
	bGestureActive = true;
}

void UMyGestureRecognizer::UpdateGesture(const FVector& Point)
{
	if (bGestureActive)
	{
		Points.Add(Point);
	}
}

void UMyGestureRecognizer::EndGesture(const FVector& End)
{
	if (!bGestureActive) return;

	Points.Add(End);
	bGestureActive = false;

	EGestureType Result = RecognizeGesture(Points);
	OnGestureRecognized.Broadcast(Result);
}

EGestureType UMyGestureRecognizer::RecognizeGesture(const TArray<FVector>& Points)
{
	if (Points.Num() < 2)
		return EGestureType::None;

	FVector StartPoint = Points[0];
	FVector EndPoint = Points.Last();

	// Calculate overall direction and distance
	FVector Direction = EndPoint - StartPoint;
	float	Distance = Direction.Size();

	// Normalize direction
	Direction.Normalize();

	// Check for swipe gestures
	if (Distance > SwipeThreshold)
	{
		if (FMath::Abs(Direction.X) > FMath::Abs(Direction.Y) && FMath::Abs(Direction.X) > FMath::Abs(Direction.Z))
		{
			return Direction.X > 0 ? EGestureType::SwipeRight : EGestureType::SwipeLeft;
		}
		else if (FMath::Abs(Direction.Y) > FMath::Abs(Direction.X) && FMath::Abs(Direction.Y) > FMath::Abs(Direction.Z))
		{
			return Direction.Y > 0 ? EGestureType::SwipeUp : EGestureType::SwipeDown;
		}
	}

	// Check for circle gesture
	if (IsCircleGesture(Points))
	{
		return EGestureType::Circle;
	}

	return EGestureType::None;
}

bool UMyGestureRecognizer::IsCircleGesture(const TArray<FVector>& Points)
{
	if (Points.Num() < 10)
		return false;

	FVector Center = CalculateCenter(Points);
	float	AverageRadius = CalculateAverageRadius(Points, Center);
	float	RadiusVariance = CalculateRadiusVariance(Points, Center, AverageRadius);

	// Check if the points form a relatively circular shape
	return RadiusVariance < 0.2f * AverageRadius;
}

FVector UMyGestureRecognizer::CalculateCenter(const TArray<FVector>& Points)
{
	FVector Sum(0, 0, 0);
	for (const FVector& Point : Points)
	{
		Sum += Point;
	}
	return Sum / Points.Num();
}

float UMyGestureRecognizer::CalculateAverageRadius(const TArray<FVector>& Points, const FVector& Center)
{
	float Sum = 0;
	for (const FVector& Point : Points)
	{
		Sum += FVector::Dist(Point, Center);
	}
	return Sum / Points.Num();
}

float UMyGestureRecognizer::CalculateRadiusVariance(const TArray<FVector>& Points, const FVector& Center, float AverageRadius)
{
	float Sum = 0;
	for (const FVector& Point : Points)
	{
		float Diff = FVector::Dist(Point, Center) - AverageRadius;
		Sum += Diff * Diff;
	}
	return FMath::Sqrt(Sum / Points.Num());
}

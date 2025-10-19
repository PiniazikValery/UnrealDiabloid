// Fill out your copyright notice in the Description page of Project Settings.

#include "UMyGestureRecognizer.h"

UMyGestureRecognizer::UMyGestureRecognizer()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UMyGestureRecognizer::StartGesture(const FVector& Start)
{
	StartLocation = Start;
	LocalPoints.Empty();
	LocalPoints.Add(Start);
	bGestureActive = true;
}

void UMyGestureRecognizer::UpdateGesture(const FVector& Point)
{
	if (bGestureActive)
	{
		LocalPoints.Add(Point);
	}
}

void UMyGestureRecognizer::EndGesture(const FVector& End)
{
	if (!bGestureActive) return;

	LocalPoints.Add(End);
	bGestureActive = false;

	EGestureType Result = RecognizeGesture(LocalPoints);
	UE_LOG(LogTemp, Warning, TEXT("GestureRecognizer: EndGesture - Result=%d"), (int32)Result);
	OnGestureRecognized.Broadcast(Result);
}

EGestureType UMyGestureRecognizer::RecognizeGesture(const TArray<FVector>& InPoints)
{
	if (InPoints.Num() < 2)
		return EGestureType::None;

	FVector StartPoint = InPoints[0];
	FVector EndPoint = InPoints.Last();

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
	if (IsCircleGesture(InPoints))
	{
		return EGestureType::Circle;
	}

	return EGestureType::None;
}

bool UMyGestureRecognizer::IsCircleGesture(const TArray<FVector>& InPoints)
{
	if (InPoints.Num() < 10)
		return false;

	FVector Center = CalculateCenter(InPoints);
	float	AverageRadius = CalculateAverageRadius(InPoints, Center);
	float	RadiusVariance = CalculateRadiusVariance(InPoints, Center, AverageRadius);

	// Check if the points form a relatively circular shape
	return RadiusVariance < 0.2f * AverageRadius;
}

FVector UMyGestureRecognizer::CalculateCenter(const TArray<FVector>& InPoints)
{
	FVector Sum(0, 0, 0);
	for (const FVector& Point : InPoints)
	{
		Sum += Point;
	}
	return Sum / InPoints.Num();
}

float UMyGestureRecognizer::CalculateAverageRadius(const TArray<FVector>& InPoints, const FVector& Center)
{
	float Sum = 0;
	for (const FVector& Point : InPoints)
	{
		Sum += FVector::Dist(Point, Center);
	}
	return Sum / InPoints.Num();
}

float UMyGestureRecognizer::CalculateRadiusVariance(const TArray<FVector>& InPoints, const FVector& Center, float AverageRadius)
{
	float Sum = 0;
	for (const FVector& Point : InPoints)
	{
		float Diff = FVector::Dist(Point, Center) - AverageRadius;
		Sum += Diff * Diff;
	}
	return FMath::Sqrt(Sum / InPoints.Num());
}

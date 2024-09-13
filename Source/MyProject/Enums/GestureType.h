// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GestureType.generated.h"

UENUM(BlueprintType)
enum class EGestureType : uint8
{
	None	   UMETA(DisplayName = "None"),
	SwipeRight UMETA(DisplayName = "SwipeRight"),
	SwipeLeft  UMETA(DisplayName = "SwipeLeft"),
	SwipeUp	   UMETA(DisplayName = "SwipeUp"),
	SwipeDown  UMETA(DisplayName = "SwipeDown"),
	Circle	   UMETA(DisplayName = "Circle"),
};
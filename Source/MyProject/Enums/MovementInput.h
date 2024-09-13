// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

// #include "CoreMinimal.h"
#include "MovementInput.generated.h"

UENUM(BlueprintType)
enum class EMovementInput : uint8
{
	Forward		  UMETA(DisplayName = "Forward"),
	Backward	  UMETA(DisplayName = "Backward"),
	Left		  UMETA(DisplayName = "Left"),
	Right		  UMETA(DisplayName = "Right"),
	ForwardLeft	  UMETA(DisplayName = "ForwardLeft"),
	ForwardRight  UMETA(DisplayName = "ForwardRight"),
	BackwardLeft  UMETA(DisplayName = "BackwardLeft"),
	BackwardRight UMETA(DisplayName = "BackwardRight"),
};
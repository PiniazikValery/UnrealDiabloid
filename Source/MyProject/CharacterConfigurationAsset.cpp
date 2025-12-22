// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterConfigurationAsset.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// Define montage name constants
const FName UCharacterConfigurationAsset::MONTAGE_START_F = TEXT("StartF");
const FName UCharacterConfigurationAsset::MONTAGE_START_R = TEXT("StartR");
const FName UCharacterConfigurationAsset::MONTAGE_ATTACK_1 = TEXT("Attack1");
const FName UCharacterConfigurationAsset::MONTAGE_ATTACK_2 = TEXT("Attack2");
const FName UCharacterConfigurationAsset::MONTAGE_DODGE = TEXT("Dodge");

UAnimMontage* UCharacterConfigurationAsset::GetAnimationMontage(FName MontageName) const
{
	const TSoftObjectPtr<UAnimMontage>* MontagePtr = AnimationMontages.Find(MontageName);
	if (MontagePtr && MontagePtr->IsValid())
	{
		return MontagePtr->Get();
	}
	
	// Try to load if not loaded
	if (MontagePtr && !MontagePtr->IsNull())
	{
		return MontagePtr->LoadSynchronous();
	}
	
	return nullptr;
}

#if WITH_EDITOR
void UCharacterConfigurationAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if WITH_EDITOR
	// Validate values when changed in editor
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();
		
		// Clamp values that need validation
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterConfigurationAsset, WalkSpeed))
		{
			WalkSpeed = FMath::Max(0.f, WalkSpeed);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterConfigurationAsset, RunSpeed))
		{
			RunSpeed = FMath::Max(0.f, RunSpeed);
			// Ensure run speed is at least as fast as walk speed
			if (RunSpeed < WalkSpeed)
			{
				RunSpeed = WalkSpeed;
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterConfigurationAsset, AirControl))
		{
			AirControl = FMath::Clamp(AirControl, 0.f, 1.f);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterConfigurationAsset, MeleeDamage))
		{
			MeleeDamage = FMath::Max(0.f, MeleeDamage);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterConfigurationAsset, MeleeRange))
		{
			MeleeRange = FMath::Max(0.f, MeleeRange);
		}
	}
#endif
}
#endif

#if WITH_EDITOR
EDataValidationResult UCharacterConfigurationAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Validate movement settings
	if (WalkSpeed <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("WalkSpeed must be greater than 0")));
		Result = EDataValidationResult::Invalid;
	}

	if (RunSpeed < WalkSpeed)
	{
		Context.AddWarning(FText::FromString(TEXT("RunSpeed should be greater than or equal to WalkSpeed")));
	}

	if (JumpVelocity <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("JumpVelocity must be greater than 0")));
		Result = EDataValidationResult::Invalid;
	}

	// Validate combat settings
	if (MeleeDamage <= 0.f)
	{
		Context.AddWarning(FText::FromString(TEXT("MeleeDamage is 0 or negative - character cannot deal damage")));
	}

	if (MeleeRange <= 0.f)
	{
		Context.AddWarning(FText::FromString(TEXT("MeleeRange is 0 or negative - melee attacks may not work")));
	}

	// Validate camera settings
	if (CameraDistance <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("CameraDistance must be greater than 0")));
		Result = EDataValidationResult::Invalid;
	}

	// Validate capsule settings
	if (CapsuleRadius <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("CapsuleRadius must be greater than 0")));
		Result = EDataValidationResult::Invalid;
	}

	if (CapsuleHalfHeight <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("CapsuleHalfHeight must be greater than 0")));
		Result = EDataValidationResult::Invalid;
	}

	// Validate required assets
	if (CharacterMesh.IsNull())
	{
		Context.AddError(FText::FromString(TEXT("CharacterMesh is not set - character will have no visual representation")));
		Result = EDataValidationResult::Invalid;
	}

	if (AnimationBlueprint == nullptr)
	{
		Context.AddWarning(FText::FromString(TEXT("AnimationBlueprint is not set - character will use default pose")));
	}

	// Validate required montages
	TArray<FName> RequiredMontages = {
		MONTAGE_START_F,
		MONTAGE_START_R,
		MONTAGE_ATTACK_1,
		MONTAGE_ATTACK_2
	};

	for (const FName& MontageName : RequiredMontages)
	{
		const TSoftObjectPtr<UAnimMontage>* MontagePtr = AnimationMontages.Find(MontageName);
		if (!MontagePtr || MontagePtr->IsNull())
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("Animation montage '%s' is not set"), *MontageName.ToString())));
		}
	}

	// Validate input assets
	if (DefaultMappingContext.IsNull())
	{
		Context.AddWarning(FText::FromString(TEXT("DefaultMappingContext is not set - input may not work")));
	}

	if (JumpAction.IsNull())
	{
		Context.AddWarning(FText::FromString(TEXT("JumpAction is not set")));
	}

	if (DodgeAction.IsNull())
	{
		Context.AddWarning(FText::FromString(TEXT("DodgeAction is not set")));
	}

	// Validate UI
	if (StatsWidgetClass == nullptr)
	{
		Context.AddWarning(FText::FromString(TEXT("StatsWidgetClass is not set - character will have no HUD")));
	}

	return Result;
}
#endif

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../MyProjectCharacter.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ProjectileSpawnNotify.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECT_API UProjectileSpawnNotify : public UAnimNotify
{
	GENERATED_BODY()

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	
};

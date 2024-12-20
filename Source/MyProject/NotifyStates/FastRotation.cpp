// Fill out your copyright notice in the Description page of Project Settings.


#include "FastRotation.h"

void UFastRotation::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		//Character->GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	}
}

void UFastRotation::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		//Character->GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	}
}

void UFastRotation::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		//Character->GetCharacterMovement()->RotationRate = FRotator(0.0f, 400.0f, 0.0f);
	}
}

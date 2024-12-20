// Fill out your copyright notice in the Description page of Project Settings.


#include "FastOrientRotationToMovement.h"

void UFastOrientRotationToMovement::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		//Character->SetRotationRate(FRotator(0.0f, 400.0f, 0.0f));
	}
}

void UFastOrientRotationToMovement::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		//Character->SetRotationRate(FRotator(0.0f, 400.0f, 0.0f));
	}
}

void UFastOrientRotationToMovement::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		//Character->SetRotationRate(FRotator(0.0f, 200.0f, 0.0f));
	}
}

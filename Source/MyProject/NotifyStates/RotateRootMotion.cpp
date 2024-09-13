#include "RotateRootMotion.h"

void URotateRootMotion::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->SetAllowPhysicsRotationDuringAnimRootMotion(true);
	}
}

void URotateRootMotion::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->SetAllowPhysicsRotationDuringAnimRootMotion(true);
	}
}

void URotateRootMotion::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->SetAllowPhysicsRotationDuringAnimRootMotion(false);
	}
}

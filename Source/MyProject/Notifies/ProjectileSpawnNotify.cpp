// Fill out your copyright notice in the Description page of Project Settings.


#include "./ProjectileSpawnNotify.h"

void UProjectileSpawnNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	AMyProjectCharacter* Character = Cast<AMyProjectCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		Character->FireProjectile();
	}
}

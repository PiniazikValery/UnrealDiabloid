// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "../AutoAimHelper.h"
#include "MageProjectile.generated.h"

UCLASS()
class MYPROJECT_API AMageProjectile : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMageProjectile();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ProjectileMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FX", meta = (AllowPrivateAccess = "true"))
	UNiagaraComponent* ProjectileNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// Damage dealt by the projectile
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float ProjectileDamage = 50.0f;

	// Radius for detecting Mass Entity enemies (since they don't have collision)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float MassEntityDamageRadius = 100.0f;

	// Target Mass Entity NetworkID (set by auto-aim system)
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	int32 TargetMassEntityNetworkID = INDEX_NONE;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// Check if projectile is close enough to hit Mass entities (they have no physics collision)
	void CheckMassEntityProximityHit();

	// Route damage through server RPC on clients, or apply directly on server
	bool ApplyMassEntityDamage(int32 NetworkID, float Damage);
	int32 ApplyAreaDamage(FVector Location, float Radius, float Damage);

	float InitialHoverHeight = 0.0f;
	float HoverAdjustTolerance = 10.0f;

	// Hit radius for Mass entity proximity detection
	float MassEntityHitRadius = 80.0f;

	// Prevent double-hit
	bool bHasHitMassEntity = false;
};

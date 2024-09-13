// Fill out your copyright notice in the Description page of Project Settings.

#include "MageProjectile.h"

// Sets default values
AMageProjectile::AMageProjectile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	RootComponent = ProjectileMesh;

	// Set the static mesh to a sphere
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereMesh.Object);
		ProjectileMesh->SetVisibility(false);

		// Calculate the scale factor
		const float DefaultRadius = 50.0f; // Default radius of the UE5 sphere
		float		ScaleFactor = 10 / DefaultRadius;

		ProjectileMesh->SetWorldScale3D(FVector(ScaleFactor));
		// Set up the collision component
		ProjectileMesh->OnComponentHit.AddDynamic(this, &AMageProjectile::OnHit);
	}

	// Find the Niagara system asset
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ProjectileNiagaraSystem(TEXT("/Script/Niagara.NiagaraSystem'/Game/Characters/Mannequins/Textures/Projectile/NS_Fireball.NS_Fireball'"));

	// Create the Niagara component and attach it to the root component
	ProjectileNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileNiagaraComponent"));
	ProjectileNiagaraComponent->SetupAttachment(RootComponent);

	// If the Niagara system was found, set it on the component
	if (ProjectileNiagaraSystem.Succeeded())
	{
		ProjectileNiagaraComponent->SetAsset(ProjectileNiagaraSystem.Object);
	}

	// static ConstructorHelpers::FObjectFinder<UMaterial> RedMaterial(TEXT("/Script/Engine.MaterialFunction'/Game/Characters/Mannequins/Materials/Functions/ML_BaseColorFallOff.ML_BaseColorFallOff'")); // Replace with your material path
	// if (RedMaterial.Succeeded())
	//{
	//	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(RedMaterial.Object, ProjectileMesh);
	//	if (MaterialInstance)
	//	{
	//		ProjectileMesh->SetMaterial(0, MaterialInstance);
	//	}
	// }

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = ProjectileMesh;
	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.3f;
}

// Called when the game starts or when spawned
void AMageProjectile::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AMageProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMageProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Check if the projectile hit a valid target (not itself)
	if (OtherActor && OtherActor != this && OtherComp)
	{
		// Optionally apply damage or other effects here

		// Destroy the projectile
		Destroy();
	}
}

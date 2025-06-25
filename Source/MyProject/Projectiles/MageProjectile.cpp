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
	SetLifeSpan(5.0f);
	FHitResult HitResult;
	FVector	   Start = GetActorLocation();
	FVector	   End = Start - FVector(0, 0, 10000);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		InitialHoverHeight = Start.Z - HitResult.ImpactPoint.Z;
	}
}

// Called every frame
void AMageProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FHitResult HitResult;
	FVector	   Start = GetActorLocation();
	FVector	   End = Start - FVector(0, 0, 10000);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		float CurrentHeight = Start.Z - HitResult.ImpactPoint.Z;

		if (FMath::Abs(CurrentHeight - InitialHoverHeight) > HoverAdjustTolerance)
		{
			FVector NewLocation = GetActorLocation();
			NewLocation.Z = HitResult.ImpactPoint.Z + InitialHoverHeight;
			SetActorLocation(NewLocation);
		}
	}
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

// Fill out your copyright notice in the Description page of Project Settings.

#include "MageProjectile.h"
#include "../EnemyCharacter.h"
#include "../MyProjectPlayerController.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"

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

		// Set collision to only hit WorldStatic/WorldDynamic, not pawns
		// This prevents projectile from pushing Mass entities (which use sweep tests)
		ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ProjectileMesh->SetCollisionObjectType(ECC_WorldDynamic);
		ProjectileMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		ProjectileMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		ProjectileMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		// Block visibility for line traces but don't block pawn movement
		ProjectileMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

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

	// Check for Mass entity proximity hit (they don't have physics collision)
	CheckMassEntityProximityHit();

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

void AMageProjectile::CheckMassEntityProximityHit()
{
	// Don't check if we already hit something
	if (bHasHitMassEntity)
	{
		return;
	}

	// Try to hit the specific target first if we have one
	if (TargetMassEntityNetworkID != INDEX_NONE)
	{
		// Get the target's position and check distance
		FMassAutoAimResult TargetInfo = UAutoAimHelper::FindBestMassEntityTarget(
			this, MassEntityHitRadius * 2.0f, 180.0f, ETargetSelectionMode::ClosestByDistance, false);

		// Check if we found our specific target nearby
		if (TargetInfo.bTargetFound && TargetInfo.TargetNetworkID == TargetMassEntityNetworkID)
		{
			float Distance = FVector::Dist(GetActorLocation(), TargetInfo.TargetLocation);
			if (Distance <= MassEntityHitRadius)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Projectile] Proximity hit on target Mass Entity NetworkID: %d at distance %.1f"),
					TargetMassEntityNetworkID, Distance);

				bHasHitMassEntity = true;
				bool bDamaged = ApplyMassEntityDamage(TargetMassEntityNetworkID, ProjectileDamage);
				UE_LOG(LogTemp, Warning, TEXT("[Projectile] Proximity damage result: %s"), bDamaged ? TEXT("SUCCESS") : TEXT("FAILED"));

				Destroy();
				return;
			}
		}
	}

	// If no specific target or target not hit, check for any nearby Mass entity
	// Only do area damage proximity check on server - clients return 1 optimistically which would
	// cause immediate projectile destruction even when no enemies are nearby
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		int32 DamagedCount = ApplyAreaDamage(GetActorLocation(), MassEntityHitRadius, ProjectileDamage);
		if (DamagedCount > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Projectile] Proximity area damage hit %d Mass entities"), DamagedCount);
			bHasHitMassEntity = true;
			Destroy();
		}
	}
}

void AMageProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogTemp, Warning, TEXT("[Projectile] OnHit called - OtherActor: %s, Location: %s, TargetNetworkID: %d"),
		OtherActor ? *OtherActor->GetName() : TEXT("NULL"),
		*Hit.ImpactPoint.ToString(),
		TargetMassEntityNetworkID);

	// Check if the projectile hit a valid target (not itself)
	if (OtherActor && OtherActor != this && OtherComp)
	{
		bool bDamagedEnemy = false;

		// Check if we hit an enemy character (Actor-based enemy)
		AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(OtherActor);
		if (EnemyCharacter)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Projectile] Hit AEnemyCharacter - applying damage"));
			// Apply damage to the enemy
			FDamageEvent DamageEvent;
			EnemyCharacter->TakeDamage(ProjectileDamage, DamageEvent, nullptr, this);
			bDamagedEnemy = true;
		}

		// If we have a specific Mass Entity target, try to damage it
		if (!bDamagedEnemy && TargetMassEntityNetworkID != INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Projectile] Attempting to damage Mass Entity NetworkID: %d with %.1f damage"),
				TargetMassEntityNetworkID, ProjectileDamage);
			bDamagedEnemy = ApplyMassEntityDamage(TargetMassEntityNetworkID, ProjectileDamage);
			UE_LOG(LogTemp, Warning, TEXT("[Projectile] ApplyDamageToMassEntity result: %s"),
				bDamagedEnemy ? TEXT("SUCCESS") : TEXT("FAILED"));
		}

		// If no specific target or didn't hit it, try area damage at hit location
		if (!bDamagedEnemy)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Projectile] Trying area damage at %s with radius %.1f"),
				*Hit.ImpactPoint.ToString(), MassEntityDamageRadius);
			int32 DamagedCount = ApplyAreaDamage(Hit.ImpactPoint, MassEntityDamageRadius, ProjectileDamage);
			UE_LOG(LogTemp, Warning, TEXT("[Projectile] Area damage hit %d enemies"), DamagedCount);
		}

		// Destroy the projectile
		Destroy();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Projectile] OnHit skipped - invalid actor or component"));
	}
}

bool AMageProjectile::ApplyMassEntityDamage(int32 NetworkID, float Damage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// On server, apply damage directly
	if (World->GetNetMode() != NM_Client)
	{
		return UAutoAimHelper::ApplyDamageToMassEntity(this, NetworkID, Damage);
	}

	// On client, route through Server RPC
	// Get the owning player's controller
	APawn* InstigatorPawn = GetInstigator();
	if (!InstigatorPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Projectile] ApplyMassEntityDamage: No instigator pawn, cannot route to server"));
		return false;
	}

	AMyProjectPlayerController* PC = Cast<AMyProjectPlayerController>(InstigatorPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Projectile] ApplyMassEntityDamage: No player controller, cannot route to server"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Projectile] Client routing damage to server via RPC - NetworkID: %d, Damage: %.1f"),
		NetworkID, Damage);

	PC->ServerApplyDamageToMassEntity(NetworkID, Damage);
	return true; // Assume success - server will handle actual damage
}

int32 AMageProjectile::ApplyAreaDamage(FVector Location, float Radius, float Damage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// On server, apply damage directly
	if (World->GetNetMode() != NM_Client)
	{
		return UAutoAimHelper::ApplyDamageAtLocation(this, Location, Radius, Damage);
	}

	// On client, route through Server RPC
	APawn* InstigatorPawn = GetInstigator();
	if (!InstigatorPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Projectile] ApplyAreaDamage: No instigator pawn, cannot route to server"));
		return 0;
	}

	AMyProjectPlayerController* PC = Cast<AMyProjectPlayerController>(InstigatorPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Projectile] ApplyAreaDamage: No player controller, cannot route to server"));
		return 0;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Projectile] Client routing area damage to server via RPC - Location: %s, Radius: %.1f, Damage: %.1f"),
		*Location.ToString(), Radius, Damage);

	PC->ServerApplyDamageAtLocation(Location, Radius, Damage);
	return 1; // Assume at least one hit - server will handle actual damage
}

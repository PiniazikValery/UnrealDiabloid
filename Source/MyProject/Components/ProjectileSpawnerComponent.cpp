#include "ProjectileSpawnerComponent.h"
#include "Components/ArrowComponent.h"
#include "Projectiles/MageProjectile.h" // simplified include; module root now in PublicIncludePaths
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"

UProjectileSpawnerComponent::UProjectileSpawnerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UProjectileSpawnerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!SpawnPoint)
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            SpawnPoint = NewObject<UArrowComponent>(Owner, TEXT("ProjectileSpawnPoint"));
            SpawnPoint->SetupAttachment(Owner->GetRootComponent());
            SpawnPoint->RegisterComponent();
            SpawnPoint->SetRelativeLocation(FVector(100.f, 0.f, 50.f));
            SpawnPoint->SetRelativeRotation(FRotator::ZeroRotator);
            SpawnPoint->bHiddenInGame = false;
        }
    }
}

void UProjectileSpawnerComponent::UpdateFromRotationOffset(float OffsetDegrees)
{
    if (!SpawnPoint) return;
    const float Rad = FMath::DegreesToRadians(OffsetDegrees + 90.f);
    SpawnPoint->SetRelativeRotation(FRotator(0.f, -OffsetDegrees, 0.f));
    SpawnPoint->SetRelativeLocation(FVector(100.f * FMath::Sin(Rad), 100.f * FMath::Cos(Rad), 50.f));
}

void UProjectileSpawnerComponent::SpawnProjectile(TSubclassOf<AMageProjectile> ProjectileClass, AActor* OwnerActor)
{
    if (!ProjectileClass || !OwnerActor || !SpawnPoint) return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World) return;
    const FVector Location = SpawnPoint->GetComponentLocation();
    const FRotator Rotation = SpawnPoint->GetComponentRotation();
    FActorSpawnParameters Params; Params.Owner = OwnerActor; Params.Instigator = Cast<APawn>(OwnerActor);
    AMageProjectile* Projectile = World->SpawnActor<AMageProjectile>(ProjectileClass, Location, Rotation, Params);
    if (Projectile && Projectile->ProjectileMovement)
    {
        const FVector Dir = Rotation.Vector();
        Projectile->ProjectileMovement->Velocity = Dir * Projectile->ProjectileMovement->InitialSpeed;
    }
}

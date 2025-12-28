// Responsible for spawning projectiles & maintaining a rotating spawn point.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectileSpawnerComponent.generated.h"

class UArrowComponent;
class AMageProjectile;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UProjectileSpawnerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UProjectileSpawnerComponent();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="Projectile")
    void SpawnProjectile(TSubclassOf<AMageProjectile> ProjectileClass, AActor* OwnerActor);

    // Spawn projectile with a specific Mass Entity target
    UFUNCTION(BlueprintCallable, Category="Projectile")
    void SpawnProjectileWithTarget(TSubclassOf<AMageProjectile> ProjectileClass, AActor* OwnerActor, int32 TargetMassEntityNetworkID);

    UFUNCTION(BlueprintCallable, Category="Projectile")
    UArrowComponent* GetSpawnPoint() const { return SpawnPoint; }

    // Update location/orientation relative to rotation offset (degrees)
    void UpdateFromRotationOffset(float OffsetDegrees);

private:
    UPROPERTY()
    UArrowComponent* SpawnPoint;
};

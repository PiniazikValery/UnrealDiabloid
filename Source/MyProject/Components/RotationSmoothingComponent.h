// Handles smooth horizontal rotation offset logic independent from character.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RotationSmoothingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotationOffsetChanged, float, NewOffsetDegrees);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URotationSmoothingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URotationSmoothingComponent();

    UFUNCTION(BlueprintCallable, Category="Rotation")
    void SmoothlyRotate(float TargetOffsetDegrees, float Speed = 1.f);

    UFUNCTION(BlueprintCallable, Category="Rotation")
    float GetCurrentOffset() const { return CurrentOffset; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(BlueprintAssignable, Category="Rotation")
    FOnRotationOffsetChanged OnRotationOffsetChanged;

private:
    float StartOffset = 0.f;
    float TargetOffset = 0.f;
    float CurrentOffset = 0.f;
    float Elapsed = 0.f;
    float SpeedScalar = 1.f; // how fast to interpolate (1 == 1 second)
    bool  bActive = false;
};

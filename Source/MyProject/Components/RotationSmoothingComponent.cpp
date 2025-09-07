// RotationSmoothingComponent.cpp
#include "RotationSmoothingComponent.h"

URotationSmoothingComponent::URotationSmoothingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void URotationSmoothingComponent::SmoothlyRotate(float TargetOffsetDegrees, float Speed)
{
    if (FMath::IsNearlyEqual(TargetOffset, TargetOffsetDegrees, 0.01f) && !bActive)
    {
        return; // already there
    }
    StartOffset = CurrentOffset;
    TargetOffset = TargetOffsetDegrees;
    SpeedScalar = FMath::Max(0.01f, Speed);
    Elapsed = 0.f;
    bActive = true;
}

void URotationSmoothingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bActive)
    {
        return;
    }
    Elapsed = FMath::Min(1.f, Elapsed + DeltaTime * SpeedScalar);
    CurrentOffset = FMath::Lerp(StartOffset, TargetOffset, Elapsed);
    OnRotationOffsetChanged.Broadcast(CurrentOffset);
    if (Elapsed >= 1.f)
    {
        bActive = false;
    }
}

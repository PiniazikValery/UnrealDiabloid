#include "MyCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"
#include "Moves/Dodge.h"

// Custom movement mode for dodge
namespace ECustomMovementMode
{
    enum Type
    {
        CMOVE_Dodge = MOVE_Custom
    };
}

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	bOrientRotationToMovement = false;
	NavMovementProperties.bUseAccelerationForPaths = true;
	bUseControllerDesiredRotation = true;
	BrakingDecelerationWalking = 512.0f;
	RotationRate = FRotator(0.0f, 0.0f, 0.0f);

    // Create dodge object and set up the movement component reference
    DodgeObject = CreateDefaultSubobject<UDodge>(TEXT("DodgeObject"));
    bIsDodging = false;
    DodgeCooldownTimer = 0.0f;
    if (DodgeObject)
    {
        DodgeObject->SetMovementComponent(this);
    }
}

void UMyCharacterMovementComponent::StartDodge()
{
    // Safety check - recreate DodgeObject if it's null or invalid
    if (!DodgeObject || !IsValid(DodgeObject))
    {
        DodgeObject = NewObject<UDodge>(this);
        if (DodgeObject)
        {
            DodgeObject->SetMovementComponent(this);
        }
        else
        {
            return;
        }
    }
    
    if(DodgeObject)
    {
        DodgeObject->StartDodge();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("StartDodge: DodgeObject is still null after recreation attempt!"));
    }
}

bool UMyCharacterMovementComponent::CanDodge() const
{
    // Safety check - ensure DodgeObject is valid
    if (!DodgeObject || !IsValid(DodgeObject))
    {
        return false;
    }
    
    return DodgeObject->CanDodge();
}

void UMyCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate cooldown timer to keep client-server synchronized
    DOREPLIFETIME(UMyCharacterMovementComponent, DodgeCooldownTimer);
    
    // Replicate dodge state for proper CanDodge synchronization
    DOREPLIFETIME(UMyCharacterMovementComponent, bIsDodging);
    
    // Replicate dodge direction so server uses same direction as client
    DOREPLIFETIME(UMyCharacterMovementComponent, DodgeDirection);
}

void UMyCharacterMovementComponent::ClientNotifyDodgeStateChanged_Implementation(bool bNewIsDodging)
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->ClientNotifyDodgeStateChanged(bNewIsDodging);
}

void UMyCharacterMovementComponent::ClientNotifyCooldownChanged_Implementation(float NewCooldown)
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->ClientNotifyCooldownChanged(NewCooldown);
}

void UMyCharacterMovementComponent::PlayDodgeMontage()
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->PlayDodgeMontage();
}

void UMyCharacterMovementComponent::MulticastPlayDodgeMontage_Implementation()
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->PlayDodgeMontage();
}

void UMyCharacterMovementComponent::DisableRotationDuringDodge()
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->DisableRotationDuringDodge();
}

void UMyCharacterMovementComponent::RestoreRotationAfterDodge()
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->RestoreRotationAfterDodge();
}

void UMyCharacterMovementComponent::RotateToDodgeDirection()
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->RotateToDodgeDirection();
}

void UMyCharacterMovementComponent::UpdateRotationBasedOnMovement(float DeltaTime)
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->UpdateRotationBasedOnMovement(DeltaTime);
}

void UMyCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    
    // Handle custom flag for dodge
    if (DodgeObject)
    {
        DodgeObject->bWantsToDodge = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    }
}

class FNetworkPredictionData_Client* UMyCharacterMovementComponent::GetPredictionData_Client() const
{
    check(PawnOwner != NULL);
    
    // Only create prediction data for clients, not the server
    if (PawnOwner->GetLocalRole() >= ROLE_Authority)
    {
        return nullptr; // Server doesn't need client prediction data
    }

    if (!ClientPredictionData)
    {
        UMyCharacterMovementComponent* MutableThis = const_cast<UMyCharacterMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_MyMovement(*this);
        MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
        MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
    }
    
    return ClientPredictionData;
}

void UMyCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
    
    // No need to send dodge direction via RPC - network prediction handles this through saved moves
}

void UMyCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    if(!DodgeObject)
    {
        Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
        return;
    }
    // Check if we should start dodging
    if (DodgeObject->bWantsToDodge)
    {
        // For clients with bad connections, be more aggressive with prediction
        bool bCanStartDodge = CanDodge();
        bool bIsClient = PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority;
        
        // Allow client to predict even if server state isn't perfectly synced
        if (bIsClient)
        {
            // For aggressive sync, only allow prediction if we're not waiting for sync
            if (!bCanStartDodge && !bIsDodging && DodgeCooldownTimer <= 0.1f && !DodgeObject->bWaitingForServerSync)
            {
                bCanStartDodge = true;
            }
        }
        
        if (bCanStartDodge)
        {
            // Only server should initiate dodge state change authoritatively
            if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
            {   
                // Rotate character to face dodge direction before disabling rotation
                RotateToDodgeDirection();
                
                // Disable rotation during dodge
                DisableRotationDuringDodge();
                
                // Initialize position tracking for dodge direction
                if (CharacterOwner)
                {
                    DodgeObject->PreviousDodgePosition = CharacterOwner->GetActorLocation();
                    DodgeObject->bHasInitializedDodgePosition = true;
                }
                
                // Set custom movement mode
                SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Dodge);
                bIsDodging = true;
                DodgeObject->DodgeTimer = DodgeObject->DodgeDuration;
                DodgeCooldownTimer = DodgeObject->DodgeCooldown;
                
                // Play dodge animation on server and all clients via multicast
                MulticastPlayDodgeMontage();
                
                // Notify client about state change
                if (ACharacter* Character = Cast<ACharacter>(PawnOwner))
                {
                    if (Character->GetController() && !Character->GetController()->IsLocalController())
                    {
                        ClientNotifyDodgeStateChanged(true);
                        
                        ClientNotifyCooldownChanged(DodgeObject->DodgeCooldown);
                    }
                }
            }
            else
            {
                // CLIENT: Start dodge immediately for client-side prediction
                // Server will validate and correct if needed

                // Rotate character to face dodge direction before disabling rotation
                RotateToDodgeDirection();

                // Disable rotation during dodge
                DisableRotationDuringDodge();

                // Initialize position tracking for dodge direction
                if (CharacterOwner)
                {
                    DodgeObject->PreviousDodgePosition = CharacterOwner->GetActorLocation();
                    DodgeObject->bHasInitializedDodgePosition = true;
                }

                // Set custom movement mode for immediate response
                SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Dodge);
                bIsDodging = true;
                DodgeObject->DodgeTimer = DodgeObject->DodgeDuration;
                DodgeCooldownTimer = DodgeObject->DodgeCooldown;

                // Play dodge animation locally
                PlayDodgeMontage();

                // Clear the flag
                DodgeObject->bWantsToDodge = false;

                // Set waiting for sync so server can correct if needed
                DodgeObject->bWaitingForServerSync = true;
                DodgeObject->LastServerSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

                UE_LOG(LogTemp, Warning, TEXT("CLIENT: Started dodge with client-side prediction"));
            }
            
            // Server clears the flag after processing
            if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
            {
                DodgeObject->bWantsToDodge = false;
            }
        }
        else
        {
            DodgeObject->bWantsToDodge = false; // Clear to prevent spam
        }
    }
    
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UMyCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
    if(!DodgeObject)
    {
        return;
    }
    
    // Update character rotation based on actual movement every tick while dodging
    if (bIsDodging)
    {
        UpdateRotationBasedOnMovement(DeltaSeconds);
    }
    
    // Update timers
    if (DodgeObject->DodgeTimer > 0.0f)
    {
        DodgeObject->DodgeTimer -= DeltaSeconds;
        if (DodgeObject->DodgeTimer <= 0.0f)
        {
            // Only server should end dodge state authoritatively
            if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
            {
                bIsDodging = false;
                DodgeObject->bWantsToDodge = false;
                
                // Reset position tracking
                DodgeObject->bHasInitializedDodgePosition = false;
                
                // Restore rotation after dodge
                RestoreRotationAfterDodge();
                
                // Ensure we're back in walking mode
                SetMovementMode(MOVE_Walking);
                
                // Help character settle on ground naturally without harsh landing
                if (Velocity.Z > 100.0f)
                {
                    Velocity.Z = 100.0f; // Cap upward velocity for smooth landing
                }
                
                // Notify client about state change
                if (ACharacter* Character = Cast<ACharacter>(PawnOwner))
                {
                    if (Character->GetController() && !Character->GetController()->IsLocalController())
                    {
                        ClientNotifyDodgeStateChanged(false);
                    }
                }
            }
            else
            {
                // CLIENT: Timer expired, but wait for server to confirm dodge end via RPC
                // Don't end dodge locally until server says so
                UE_LOG(LogTemp, Warning, TEXT("CLIENT: Dodge timer expired locally - waiting for server confirmation"));
            }
        }
        else if (DodgeObject->DodgeTimer > 0.0f && PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority)
        {
            // Client updates timer locally for visual feedback
            // But actual dodge end is controlled by server RPC
        }
    }
    
    // Only server updates cooldown timer
    if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
    {
        if (DodgeCooldownTimer > 0.0f)
        {
            DodgeCooldownTimer -= DeltaSeconds;
            if (DodgeCooldownTimer <= 0.0f)
            {
                DodgeCooldownTimer = 0.0f; // Ensure it's exactly 0
                
                // Notify client when cooldown expires
                if (ACharacter* Character = Cast<ACharacter>(PawnOwner))
                {
                    if (Character->GetController() && !Character->GetController()->IsLocalController())
                    {
                        ClientNotifyCooldownChanged(0.0f);
                    }
                }
            }
        }
    }
    // Client does NOT update cooldown - waits for server RPC
    
    // Safety check to prevent infinite dodge loops
    if (DodgeObject->DodgeTimer <= 0.0f)
    {
        if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
        {
            if (bIsDodging)
            {
                DodgeObject->bHasInitializedDodgePosition = false;
                RestoreRotationAfterDodge();
                bIsDodging = false;
                DodgeObject->bWantsToDodge = false;
                SetMovementMode(MOVE_Walking);
            }
        }
        else
        {
            // Client should predict state changes for immediate feedback
            if (MovementMode == MOVE_Custom && !DodgeObject->bClientHasPredictedDodgeEnd)
            {
                DodgeObject->bHasInitializedDodgePosition = false;
                RestoreRotationAfterDodge();
                bIsDodging = false; // Update client state immediately
                SetMovementMode(MOVE_Walking);
                DodgeObject->bClientHasPredictedDodgeEnd = true; // Mark that we've ended the dodge
            }
        }
    }
}

void UMyCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    Super::PhysCustom(deltaTime, Iterations);
    if(!DodgeObject)
    {
        return;
    }
    
    switch (CustomMovementMode)
    {
        case ECustomMovementMode::CMOVE_Dodge:
        {   
            // Calculate dodge velocity
            FVector DodgeVel = DodgeDirection * DodgeObject->DodgeSpeed;
            
            // Apply ground multiplier if on ground
            if (IsMovingOnGround())
            {
                DodgeVel *= DodgeObject->GroundDodgeMultiplier;
            }
            
            // Keep character close to ground but add slight upward velocity to avoid getting stuck
            // This creates a "sliding" effect rather than flying
            DodgeVel.Z = FMath::Max(DodgeVel.Z, DodgeObject->DodgeGroundClearance); // Small upward velocity to clear tiny obstacles
            
            // Set velocity directly for instant dodge
            Velocity = DodgeVel;
            
            // Stay in walking mode but with custom physics to handle ground properly
            // This keeps the character grounded while avoiding friction issues
            if (MovementMode != MOVE_Walking)
            {
                SetMovementMode(MOVE_Walking);
            }
            
            // Perform movement with ground-hugging behavior
            const FVector Adjusted = Velocity * deltaTime;
            FHitResult Hit(1.f);
            
            // Move with collision detection
            SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
            
            // If we hit something, slide along it while staying grounded
            if (Hit.IsValidBlockingHit())
            {
                FVector SlideVel = FVector::VectorPlaneProject(Velocity, Hit.Normal);
                // Don't lose too much Z velocity to stay slightly above ground
                SlideVel.Z = FMath::Max(SlideVel.Z, DodgeObject->DodgeGroundClearance * 0.5f); 
                SlideVel *= DodgeObject->DodgeWallSlideFactor; // Reduce velocity when hitting walls
                Velocity = SlideVel;
                
                // Continue sliding movement
                const FVector SlideAdjusted = Velocity * deltaTime * (1.0f - Hit.Time);
                FHitResult SlideHit(1.f);
                SafeMoveUpdatedComponent(SlideAdjusted, UpdatedComponent->GetComponentQuat(), true, SlideHit);
            }
            
            // Apply gravity-like effect to pull character toward ground during dodge
            if (!IsMovingOnGround())
            {
                Velocity.Z -= GetGravityZ() * deltaTime * 0.5f; // Reduced gravity for smoother movement
            }
            
            break;
        }
        default:
            SetMovementMode(MOVE_Walking);
            break;
    }
}

// Removed ServerSetDodgeDirection RPC functions - network prediction handles dodge direction through saved moves

void UMyCharacterMovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	// When bIgnoreServerCorrections is true, skip applying server corrections entirely
	// This allows the client to remain smooth during attacks even with bad connections
	if (bIgnoreServerCorrections)
	{
		return;
	}

	// Otherwise, apply normal correction
	Super::SmoothCorrection(OldLocation, OldRotation, NewLocation, NewRotation);
}

void UMyCharacterMovementComponent::ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewVel, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, TOptional<FRotator> OptionalRotation)
{
	// When bIgnoreServerCorrections is true, completely ignore server position corrections
	// The client is fully autonomous - server cannot adjust our position
	if (bIgnoreServerCorrections)
	{
		return;
	}

	// Otherwise, apply normal server correction
	Super::ClientAdjustPosition_Implementation(TimeStamp, NewLoc, NewVel, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, OptionalRotation);
}

void UMyCharacterMovementComponent::ServerStartDodge_Implementation(const FVector& Direction)
{
    if(!DodgeObject)
    {
        return;
    }
    DodgeObject->ServerStartDodge(Direction);
}

bool UMyCharacterMovementComponent::ServerStartDodge_Validate(const FVector& Direction)
{
    // Basic validation - direction should be normalized or zero
    return Direction.SizeSquared() <= 1.1f; // Allow small tolerance for normalization
}

//==========================================================================================
// FSavedMove_MyMovement Implementation
//==========================================================================================

void FSavedMove_MyMovement::Clear()
{
    Super::Clear();
    
    bSavedWantsToDodge = false;
    SavedDodgeDirection = FVector::ZeroVector;
    SavedDodgeTimer = 0.0f;
    SavedDodgeCooldownTimer = 0.0f;
}

uint8 FSavedMove_MyMovement::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();
    
    if (bSavedWantsToDodge)
    {
        Result |= FLAG_Custom_0;
    }
    
    return Result;
}

bool FSavedMove_MyMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
    const FSavedMove_MyMovement* NewMyMove = static_cast<const FSavedMove_MyMovement*>(NewMove.Get());
    
    if (bSavedWantsToDodge != NewMyMove->bSavedWantsToDodge)
        return false;
        
    if (!SavedDodgeDirection.Equals(NewMyMove->SavedDodgeDirection, 0.01f))
        return false;
        
    if (FMath::Abs(SavedDodgeTimer - NewMyMove->SavedDodgeTimer) > 0.01f)
        return false;
    
    return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_MyMovement::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, 
                                       class FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
    
    UMyCharacterMovementComponent* CharMov = Cast<UMyCharacterMovementComponent>(Character->GetCharacterMovement());
    if (CharMov)
    {
        bSavedWantsToDodge = CharMov->DodgeObject->bWantsToDodge;
        SavedDodgeDirection = CharMov->DodgeDirection;
        SavedDodgeTimer = CharMov->DodgeObject->DodgeTimer;
        SavedDodgeCooldownTimer = CharMov->DodgeCooldownTimer;
    }
}

void FSavedMove_MyMovement::PrepMoveFor(ACharacter* Character)
{
    Super::PrepMoveFor(Character);
    
    UMyCharacterMovementComponent* CharMov = Cast<UMyCharacterMovementComponent>(Character->GetCharacterMovement());
    if (CharMov)
    {
        // Restore saved state normally - keep the loop prevention simpler
        CharMov->DodgeObject->bWantsToDodge = bSavedWantsToDodge;
        CharMov->DodgeDirection = SavedDodgeDirection;
        CharMov->DodgeObject->DodgeTimer = SavedDodgeTimer;
        CharMov->DodgeCooldownTimer = SavedDodgeCooldownTimer;
    }
}

//==========================================================================================
// FNetworkPredictionData_Client_MyMovement Implementation
//==========================================================================================

FNetworkPredictionData_Client_MyMovement::FNetworkPredictionData_Client_MyMovement(const UCharacterMovementComponent& ClientMovement)
    : Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_MyMovement::AllocateNewMove()
{
    return FSavedMovePtr(new FSavedMove_MyMovement());
}

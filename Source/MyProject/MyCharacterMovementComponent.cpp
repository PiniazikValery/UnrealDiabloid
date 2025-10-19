#include "MyCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

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

    // Default dodge values
    DodgeSpeed = 1900.0f;
    DodgeDuration = 0.5f;
    DodgeCooldown = 1.0f;
    GroundDodgeMultiplier = 1.2f;
    DodgeGroundClearance = 50.0f;  // Small clearance for ground obstacles
    DodgeWallSlideFactor = 0.7f;
    
    // Load dodge montage
    static ConstructorHelpers::FObjectFinder<UAnimMontage> DodgeMontageAsset(TEXT("/Script/Engine.AnimMontage'/Game/Characters/Mannequins/Animations/Locomotion/Dodge/Dodge_Montage.Dodge_Montage'"));
    if (DodgeMontageAsset.Succeeded())
    {
        DodgeMontage = DodgeMontageAsset.Object;
    }
    
    bIsDodging = false;
    bWantsToDodge = false;
    DodgeTimer = 0.0f;
    DodgeCooldownTimer = 0.0f;
    LastDodgeRPCTime = 0.0f;
    bClientHasPredictedDodgeEnd = false;
    
    // Network sync tracking for dodge timing
    LastServerSyncTime = 0.0f;
    bWaitingForServerSync = false;
    ServerSyncWindow = 0.2f; // More aggressive default sync window
    
    // Initialize saved rotation settings
    bSavedOrientRotationToMovement = false;
    bSavedUseControllerDesiredRotation = false;
    SavedRotationRate = FRotator::ZeroRotator;
    
    // Initialize dodge position tracking
    PreviousDodgePosition = FVector::ZeroVector;
    bHasInitializedDodgePosition = false;
}

void UMyCharacterMovementComponent::StartDodge()
{
    UE_LOG(LogTemp, Warning, TEXT("StartDodge called - CanDodge(): %s"), CanDodge() ? TEXT("true") : TEXT("false"));
    
    // Check if we're waiting for server sync after previous dodge
    if (bWaitingForServerSync && PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority)
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        float TimeSinceLastSync = CurrentTime - LastServerSyncTime;
        
        if (TimeSinceLastSync < ServerSyncWindow)
        {
            UE_LOG(LogTemp, Warning, TEXT("StartDodge rejected - waiting for server sync (%.3f/%.3f seconds)"), 
                TimeSinceLastSync, ServerSyncWindow);
            return;
        }
        else
        {
            // Sync window expired, allow dodge but adjust window based on network quality
            bWaitingForServerSync = false;
            UE_LOG(LogTemp, Warning, TEXT("Sync window expired - allowing dodge and adjusting sync window"));
            
            // Adapt sync window based on how long we waited
            if (TimeSinceLastSync > ServerSyncWindow * 1.5f)
            {
                // Bad connection - increase sync window more aggressively
                ServerSyncWindow = FMath::Min(ServerSyncWindow * 1.5f, 0.5f);
                UE_LOG(LogTemp, Warning, TEXT("Bad connection detected - increased sync window to %.3f"), ServerSyncWindow);
            }
            else if (TimeSinceLastSync < ServerSyncWindow * 0.6f)
            {
                // Good connection - decrease sync window more slowly
                ServerSyncWindow = FMath::Max(ServerSyncWindow * 0.95f, 0.1f);
                UE_LOG(LogTemp, Warning, TEXT("Good connection detected - decreased sync window to %.3f"), ServerSyncWindow);
            }
        }
    }
    
    // For immediate responsiveness, allow client to start new dodge if current dodge is almost finished
    bool bAllowImmediateDodge = false;
    if (PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority && bIsDodging && DodgeTimer <= 0.05f && !bWaitingForServerSync)
    {
        // Client can predict immediate dodge if current dodge is finishing very soon and we're not waiting for sync
        bAllowImmediateDodge = true;
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Allowing immediate dodge - current dodge almost finished (%.3f seconds left)"), DodgeTimer);
    }
    
    // Simplified validation - only check if already dodging (unless immediate dodge allowed)
    if (bIsDodging && !bAllowImmediateDodge)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDodge rejected - already dodging"));
        return;
    }
    
    // Additional check for cooldown to prevent spam (with small tolerance for immediate chaining)
    float CooldownTolerance = bAllowImmediateDodge ? 0.1f : 0.0f;
    if (DodgeCooldownTimer > CooldownTolerance)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDodge rejected - still on cooldown: %f (tolerance: %f)"), DodgeCooldownTimer, CooldownTolerance);
        return;
    }
    
    // Check if we're already wanting to dodge (prevents spam)
    if (bWantsToDodge && !bAllowImmediateDodge)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDodge rejected - already wanting to dodge"));
        return;
    }
    
    // For bad connections, be more aggressive with client prediction
    bool bCanDodgeOrChain = CanDodge() || bAllowImmediateDodge;
    
    if (bCanDodgeOrChain)
    {
        UE_LOG(LogTemp, Warning, TEXT("Setting bWantsToDodge to true (CanDodge: %s, AllowImmediate: %s)"), 
            CanDodge() ? TEXT("true") : TEXT("false"), bAllowImmediateDodge ? TEXT("true") : TEXT("false"));
        
        // Calculate dodge direction immediately
        if (PawnOwner)
        {
            DodgeDirection = PawnOwner->GetLastMovementInputVector();
            if (DodgeDirection.IsZero())
            {
                // Dodge forward if no input
                DodgeDirection = CharacterOwner ? CharacterOwner->GetActorForwardVector() : FVector::ForwardVector;
            }
            DodgeDirection.Normalize();
            
            UE_LOG(LogTemp, Warning, TEXT("StartDodge - Setting DodgeDirection: %s"), *DodgeDirection.ToString());
        }
        
        // If we're a client, send dodge direction to server immediately
        if (PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority)
        {
            // Only send RPC if we have a valid network connection
            if (PawnOwner->GetNetConnection() != nullptr)
            {
                // For bad connections, reduce rate limiting for more responsive chaining
                float RateLimit = bAllowImmediateDodge ? 0.05f : 0.1f; // Faster rate for chaining
                float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
                if (CurrentTime - LastDodgeRPCTime >= RateLimit)
                {
                    // Send dodge request to server with direction
                    ServerStartDodge(DodgeDirection);
                    LastDodgeRPCTime = CurrentTime;
                    UE_LOG(LogTemp, Warning, TEXT("CLIENT: Sent ServerStartDodge with direction: %s (rate limit: %f)"), 
                        *DodgeDirection.ToString(), RateLimit);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("CLIENT: ServerStartDodge rate limited - last RPC was %f seconds ago (limit: %f)"), 
                        CurrentTime - LastDodgeRPCTime, RateLimit);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("CLIENT: No network connection - skipping ServerStartDodge RPC"));
            }
            
            // For bad connections, immediately start client prediction for responsiveness
            if (bAllowImmediateDodge)
            {
                UE_LOG(LogTemp, Warning, TEXT("CLIENT: Bad connection compensation - immediate prediction start"));
                // Force end current dodge for immediate chaining
                if (bIsDodging)
                {
                    bIsDodging = false;
                    SetMovementMode(MOVE_Walking);
                    DodgeTimer = 0.0f;
                    bClientHasPredictedDodgeEnd = true;
                }
                // Reset for new dodge
                bClientHasPredictedDodgeEnd = false;
                DodgeCooldownTimer = 0.0f; // Allow immediate chaining on client
            }
        }
        
        bWantsToDodge = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot dodge - bIsDodging: %s, DodgeCooldownTimer: %f, bWantsToDodge: %s"), 
            bIsDodging ? TEXT("true") : TEXT("false"),
            DodgeCooldownTimer,
            bWantsToDodge ? TEXT("true") : TEXT("false"));
    }
}

bool UMyCharacterMovementComponent::CanDodge() const
{
    // Server has authority over cooldown validation
    if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
    {
        // Server-side validation is authoritative
        bool bResult = !bIsDodging && DodgeCooldownTimer <= 0.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("CanDodge SERVER check - bIsDodging: %s (replicated), DodgeCooldownTimer: %f (replicated), Result: %s"),
            bIsDodging ? TEXT("true") : TEXT("false"),
            DodgeCooldownTimer,
            bResult ? TEXT("true") : TEXT("false"));
            
        return bResult;
    }
    else
    {
        // Client uses replicated values from server but allows for smoother chaining
        bool bBasicResult = !bIsDodging && DodgeCooldownTimer <= 0.0f && !bWaitingForServerSync;
        
        // For immediate responsiveness, also allow if current dodge is almost finished (smaller window for more aggressive sync)
        bool bChainResult = bIsDodging && DodgeTimer <= 0.05f && DodgeCooldownTimer <= 0.1f && !bWaitingForServerSync;
        
        bool bResult = bBasicResult || bChainResult;
        
        UE_LOG(LogTemp, Warning, TEXT("CanDodge CLIENT check - bIsDodging: %s, DodgeCooldownTimer: %f, DodgeTimer: %f, bWaitingForServerSync: %s, BasicResult: %s, ChainResult: %s, FinalResult: %s"),
            bIsDodging ? TEXT("true") : TEXT("false"),
            DodgeCooldownTimer,
            DodgeTimer,
            bWaitingForServerSync ? TEXT("true") : TEXT("false"),
            bBasicResult ? TEXT("true") : TEXT("false"),
            bChainResult ? TEXT("true") : TEXT("false"),
            bResult ? TEXT("true") : TEXT("false"));
            
        return bResult;
    }
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
    UE_LOG(LogTemp, Warning, TEXT("CLIENT: ClientNotifyDodgeStateChanged called - bNewIsDodging: %s"), bNewIsDodging ? TEXT("true") : TEXT("false"));
    
    if (bWaitingForServerSync)
    {
        bWaitingForServerSync = false;
        LastServerSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server sync detected via dodge state change - sync complete"));
        
        // More conservative sync window reduction for aggressive sync
        ServerSyncWindow = FMath::Max(ServerSyncWindow * 0.98f, 0.1f);
    }
    
    // Sync dodge state and movement mode with server
    if (bNewIsDodging)
    {
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server confirmed dodge start - starting dodge on client"));
        
        // Rotate character to face dodge direction before disabling rotation
        RotateToDodgeDirection();
        
        // Disable rotation during dodge
        DisableRotationDuringDodge();
        
        // Initialize position tracking for dodge direction
        if (CharacterOwner)
        {
            PreviousDodgePosition = CharacterOwner->GetActorLocation();
            bHasInitializedDodgePosition = true;
        }
        
        // Start dodge on client now that server has confirmed
        bIsDodging = true;
        DodgeTimer = DodgeDuration;
        bClientHasPredictedDodgeEnd = false;
        SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Dodge);
        
        // Play dodge animation on client
        PlayDodgeMontage();
        
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Dodge started after server confirmation - DodgeTimer: %f"), DodgeTimer);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server confirmed dodge end - ending dodge on client"));
        
        // Reset position tracking
        bHasInitializedDodgePosition = false;
        
        // Restore rotation after dodge
        RestoreRotationAfterDodge();
        
        // End dodge on client
        bIsDodging = false;
        bClientHasPredictedDodgeEnd = false;
        SetMovementMode(MOVE_Walking);
        
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Dodge ended after server confirmation"));
    }
}

void UMyCharacterMovementComponent::ClientNotifyCooldownChanged_Implementation(float NewCooldown)
{
    UE_LOG(LogTemp, Warning, TEXT("CLIENT: ClientNotifyCooldownChanged called - NewCooldown: %f"), NewCooldown);
    
    if (bWaitingForServerSync)
    {
        bWaitingForServerSync = false;
        LastServerSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server sync detected via cooldown change - sync complete"));
        
        // More conservative sync window reduction for aggressive sync
        ServerSyncWindow = FMath::Max(ServerSyncWindow * 0.98f, 0.1f);
    }
    
    // Sync cooldown with server's value
    DodgeCooldownTimer = NewCooldown;
    
    // Log cooldown state for debugging
    if (NewCooldown <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server confirmed cooldown expired - can dodge again"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Cooldown set to %f seconds"), NewCooldown);
    }
}

void UMyCharacterMovementComponent::PlayDodgeMontage()
{
    if (!DodgeMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: DodgeMontage is null!"));
        return;
    }
    
    if (ACharacter* Character = Cast<ACharacter>(PawnOwner))
    {
        if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
        {
            if (!AnimInstance->Montage_IsPlaying(DodgeMontage))
            {
                AnimInstance->Montage_Play(DodgeMontage, 1.0f);
                UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: Playing dodge animation montage"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: Montage already playing"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: AnimInstance is null!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: Character is null!"));
    }
}

void UMyCharacterMovementComponent::DisableRotationDuringDodge()
{
    // Save current rotation settings
    bSavedOrientRotationToMovement = bOrientRotationToMovement;
    bSavedUseControllerDesiredRotation = bUseControllerDesiredRotation;
    SavedRotationRate = RotationRate;
    
    // Disable all rotation
    bOrientRotationToMovement = false;
    bUseControllerDesiredRotation = false;
    RotationRate = FRotator::ZeroRotator;
    
    UE_LOG(LogTemp, Warning, TEXT("DisableRotationDuringDodge: Rotation disabled"));
}

void UMyCharacterMovementComponent::RestoreRotationAfterDodge()
{
    // Restore saved rotation settings
    bOrientRotationToMovement = bSavedOrientRotationToMovement;
    bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
    RotationRate = SavedRotationRate;
    
    UE_LOG(LogTemp, Warning, TEXT("RestoreRotationAfterDodge: Rotation restored (OrientToMovement: %s, UseControllerDesired: %s, Rate: %s)"), 
        bOrientRotationToMovement ? TEXT("true") : TEXT("false"),
        bUseControllerDesiredRotation ? TEXT("true") : TEXT("false"),
        *RotationRate.ToString());
}

void UMyCharacterMovementComponent::RotateToDodgeDirection()
{
    if (!DodgeDirection.IsZero() && CharacterOwner)
    {
        // Calculate rotation to face dodge direction (ignore Z component for yaw-only rotation)
        FVector FlatDodgeDirection = DodgeDirection;
        FlatDodgeDirection.Z = 0.0f;
        FlatDodgeDirection.Normalize();
        
        if (!FlatDodgeDirection.IsZero())
        {
            // Create rotation from direction vector
            FRotator TargetRotation = FlatDodgeDirection.Rotation();
            
            // Instantly set the character's rotation to face dodge direction
            CharacterOwner->SetActorRotation(TargetRotation);
            
            UE_LOG(LogTemp, Warning, TEXT("RotateToDodgeDirection: Rotated character to %s (dodge direction: %s)"), 
                *TargetRotation.ToString(), *DodgeDirection.ToString());
        }
    }
}

void UMyCharacterMovementComponent::UpdateRotationBasedOnMovement(float DeltaTime)
{
    if (!bIsDodging || !CharacterOwner)
    {
        return;
    }
    
    // Get current position
    FVector CurrentPosition = CharacterOwner->GetActorLocation();
    
    // If we haven't initialized yet, just store the current position and return
    if (!bHasInitializedDodgePosition)
    {
        PreviousDodgePosition = CurrentPosition;
        bHasInitializedDodgePosition = true;
        return;
    }
    
    // Calculate movement direction based on position change
    FVector MovementDelta = CurrentPosition - PreviousDodgePosition;
    
    // Only update rotation if there was significant movement
    float MovementDistanceSq = MovementDelta.SizeSquared();
    const float MinMovementThreshold = 1.0f; // Minimum movement in cm squared (1cm = 1 unit in Unreal)
    
    if (MovementDistanceSq > MinMovementThreshold)
    {
        // Normalize and flatten the movement direction (ignore Z for yaw-only rotation)
        FVector FlatMovementDirection = MovementDelta;
        FlatMovementDirection.Z = 0.0f;
        FlatMovementDirection.Normalize();
        
        if (!FlatMovementDirection.IsZero())
        {
            // Create rotation from actual movement direction
            FRotator TargetRotation = FlatMovementDirection.Rotation();
            
            // Smoothly rotate character to face actual movement direction
            // Use a fast rotation rate for responsive dodge direction changes
            float RotationSpeed = 720.0f; // Degrees per second
            FRotator CurrentRotation = CharacterOwner->GetActorRotation();
            FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
            
            CharacterOwner->SetActorRotation(NewRotation);
            
            UE_LOG(LogTemp, Verbose, TEXT("UpdateRotationBasedOnMovement: Movement delta: %s, Target rotation: %s"), 
                *MovementDelta.ToString(), *TargetRotation.ToString());
        }
    }
    
    // Store current position for next frame
    PreviousDodgePosition = CurrentPosition;
}

void UMyCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    
    // Handle custom flag for dodge
    bWantsToDodge = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
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
    // Check if we should start dodging
    if (bWantsToDodge)
    {
        // For clients with bad connections, be more aggressive with prediction
        bool bCanStartDodge = CanDodge();
        bool bIsClient = PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority;
        
        // Allow client to predict even if server state isn't perfectly synced
        if (bIsClient)
        {
            // For aggressive sync, only allow prediction if we're not waiting for sync
            if (!bCanStartDodge && !bIsDodging && DodgeCooldownTimer <= 0.1f && !bWaitingForServerSync)
            {
                bCanStartDodge = true;
                UE_LOG(LogTemp, Warning, TEXT("CLIENT: Aggressive sync compensation - allowing dodge despite cooldown: %f"), DodgeCooldownTimer);
            }
        }
        
        if (bCanStartDodge)
        {
            // Only server should initiate dodge state change authoritatively
            if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
            {
                UE_LOG(LogTemp, Warning, TEXT("SERVER: Starting dodge - setting custom movement mode"));
                
                // Rotate character to face dodge direction before disabling rotation
                RotateToDodgeDirection();
                
                // Disable rotation during dodge
                DisableRotationDuringDodge();
                
                // Initialize position tracking for dodge direction
                if (CharacterOwner)
                {
                    PreviousDodgePosition = CharacterOwner->GetActorLocation();
                    bHasInitializedDodgePosition = true;
                }
                
                // Set custom movement mode
                SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Dodge);
                bIsDodging = true;
                DodgeTimer = DodgeDuration;
                DodgeCooldownTimer = DodgeCooldown;
                
                // Play dodge animation on server
                PlayDodgeMontage();
                
                // Notify client about state change
                if (ACharacter* Character = Cast<ACharacter>(PawnOwner))
                {
                    if (Character->GetController() && !Character->GetController()->IsLocalController())
                    {
                        ClientNotifyDodgeStateChanged(true);
                        ClientNotifyCooldownChanged(DodgeCooldown);
                        UE_LOG(LogTemp, Warning, TEXT("SERVER: Sent ClientNotifyDodgeStateChanged(true) and ClientNotifyCooldownChanged(%f)"), DodgeCooldown);
                    }
                }
                
                UE_LOG(LogTemp, Warning, TEXT("SERVER: Set cooldown timer to %f"), DodgeCooldown);
                UE_LOG(LogTemp, Warning, TEXT("SERVER: Dodge started - bWantsToDodge set to false, DodgeTimer: %f"), DodgeTimer);
            }
            else
            {
                // CLIENT: Don't start dodge until server confirms
                // Just clear the flag and wait for server RPC to actually start the dodge
                UE_LOG(LogTemp, Warning, TEXT("CLIENT: Dodge request sent to server - waiting for confirmation before starting"));
                bWantsToDodge = false; // Clear flag immediately
                
                // Set waiting for sync so we don't spam requests
                bWaitingForServerSync = true;
                LastServerSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            }
            
            // Server clears the flag after processing
            if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
            {
                bWantsToDodge = false;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot start dodge - bCanStartDodge: %s, bIsDodging: %s, DodgeCooldownTimer: %f"), 
                bCanStartDodge ? TEXT("true") : TEXT("false"),
                bIsDodging ? TEXT("true") : TEXT("false"),
                DodgeCooldownTimer);
            bWantsToDodge = false; // Clear to prevent spam
        }
    }
    
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UMyCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
    
    // Update character rotation based on actual movement every tick while dodging
    if (bIsDodging)
    {
        UpdateRotationBasedOnMovement(DeltaSeconds);
    }
    
    // Update timers
    if (DodgeTimer > 0.0f)
    {
        DodgeTimer -= DeltaSeconds;
        if (DodgeTimer <= 0.0f)
        {
            // Only server should end dodge state authoritatively
            if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
            {
                UE_LOG(LogTemp, Warning, TEXT("SERVER: Dodge timer expired - ending dodge"));
                bIsDodging = false;
                bWantsToDodge = false;
                
                // Reset position tracking
                bHasInitializedDodgePosition = false;
                
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
                        UE_LOG(LogTemp, Warning, TEXT("SERVER: Sent ClientNotifyDodgeStateChanged(false)"));
                    }
                }
                
                UE_LOG(LogTemp, Warning, TEXT("SERVER: Dodge ended - bIsDodging: false, bWantsToDodge: false"));
            }
            else
            {
                // CLIENT: Timer expired, but wait for server to confirm dodge end via RPC
                // Don't end dodge locally until server says so
                UE_LOG(LogTemp, Warning, TEXT("CLIENT: Dodge timer expired locally - waiting for server confirmation"));
            }
        }
        else if (DodgeTimer > 0.0f && PawnOwner && PawnOwner->GetLocalRole() < ROLE_Authority)
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
                UE_LOG(LogTemp, Warning, TEXT("SERVER: Dodge cooldown expired"));
                DodgeCooldownTimer = 0.0f; // Ensure it's exactly 0
                
                // Notify client when cooldown expires
                if (ACharacter* Character = Cast<ACharacter>(PawnOwner))
                {
                    if (Character->GetController() && !Character->GetController()->IsLocalController())
                    {
                        ClientNotifyCooldownChanged(0.0f);
                        UE_LOG(LogTemp, Warning, TEXT("SERVER: Sent ClientNotifyCooldownChanged(0.0)"));
                    }
                }
            }
        }
    }
    // Client does NOT update cooldown - waits for server RPC
    
    // Safety check to prevent infinite dodge loops
    if (DodgeTimer <= 0.0f)
    {
        if (PawnOwner && PawnOwner->GetLocalRole() == ROLE_Authority)
        {
            if (bIsDodging)
            {
                UE_LOG(LogTemp, Error, TEXT("SERVER SAFETY: Force ending dodge that should have ended"));
                bHasInitializedDodgePosition = false;
                RestoreRotationAfterDodge();
                bIsDodging = false;
                bWantsToDodge = false;
                SetMovementMode(MOVE_Walking);
            }
        }
        else
        {
            // Client should predict state changes for immediate feedback
            if (MovementMode == MOVE_Custom && !bClientHasPredictedDodgeEnd)
            {
                UE_LOG(LogTemp, Error, TEXT("CLIENT SAFETY: Force ending custom movement mode"));
                bHasInitializedDodgePosition = false;
                RestoreRotationAfterDodge();
                bIsDodging = false; // Update client state immediately
                SetMovementMode(MOVE_Walking);
                bClientHasPredictedDodgeEnd = true; // Mark that we've ended the dodge
            }
        }
    }
}

void UMyCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    Super::PhysCustom(deltaTime, Iterations);
    
    switch (CustomMovementMode)
    {
        case ECustomMovementMode::CMOVE_Dodge:
        {
            UE_LOG(LogTemp, Warning, TEXT("PhysCustom - CMOVE_Dodge executing"));
            
            // Calculate dodge velocity
            FVector DodgeVel = DodgeDirection * DodgeSpeed;
            
            // Apply ground multiplier if on ground
            if (IsMovingOnGround())
            {
                DodgeVel *= GroundDodgeMultiplier;
            }
            
            // Keep character close to ground but add slight upward velocity to avoid getting stuck
            // This creates a "sliding" effect rather than flying
            DodgeVel.Z = FMath::Max(DodgeVel.Z, DodgeGroundClearance); // Small upward velocity to clear tiny obstacles
            
            UE_LOG(LogTemp, Warning, TEXT("Dodge velocity: %s"), *DodgeVel.ToString());
            
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
                SlideVel.Z = FMath::Max(SlideVel.Z, DodgeGroundClearance * 0.5f); 
                SlideVel *= DodgeWallSlideFactor; // Reduce velocity when hitting walls
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
            UE_LOG(LogTemp, Warning, TEXT("Invalid custom movement mode"));
            SetMovementMode(MOVE_Walking);
            break;
    }
}

// Removed ServerSetDodgeDirection RPC functions - network prediction handles dodge direction through saved moves

void UMyCharacterMovementComponent::ServerStartDodge_Implementation(const FVector& Direction)
{
    UE_LOG(LogTemp, Warning, TEXT("ServerStartDodge_Implementation called with Direction: %s"), *Direction.ToString());
    
    // More lenient server-side validation for smoother chaining (but still aggressive sync)
    bool bAllowChaining = bIsDodging && DodgeTimer <= 0.05f && DodgeCooldownTimer <= 0.1f;
    
    // Validate server-side to prevent spam but allow chaining
    if (bIsDodging && !bAllowChaining)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - already dodging (no chaining allowed)"));
        return;
    }
    
    if (DodgeCooldownTimer > 0.1f) // Smaller tolerance for aggressive sync
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - still on cooldown: %f"), DodgeCooldownTimer);
        return;
    }
    
    if (bWantsToDodge && !bAllowChaining)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - already wanting to dodge"));
        return;
    }
    
    // If we're chaining, force end the current dodge first
    if (bAllowChaining)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: Allowing dodge chaining - ending current dodge first"));
        bIsDodging = false;
        bWantsToDodge = false;
        SetMovementMode(MOVE_Walking);
        DodgeTimer = 0.0f;
        DodgeCooldownTimer = 0.0f; // Reset cooldown for chaining
    }
    
    // Set the dodge direction on server
    DodgeDirection = Direction;
    
    // Set bWantsToDodge so the server will process the dodge in UpdateCharacterStateBeforeMovement
    if (CanDodge() || bAllowChaining)
    {
        bWantsToDodge = true;
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge set bWantsToDodge=true with direction: %s (chaining: %s)"), 
            *DodgeDirection.ToString(), bAllowChaining ? TEXT("true") : TEXT("false"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - CanDodge() returned false"));
    }
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
        bSavedWantsToDodge = CharMov->bWantsToDodge;
        SavedDodgeDirection = CharMov->DodgeDirection;
        SavedDodgeTimer = CharMov->DodgeTimer;
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
        CharMov->bWantsToDodge = bSavedWantsToDodge;
        CharMov->DodgeDirection = SavedDodgeDirection;
        CharMov->DodgeTimer = SavedDodgeTimer;
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

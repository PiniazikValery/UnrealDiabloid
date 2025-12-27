#include "Dodge.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"
#include "../MyCharacterMovementComponent.h"

UDodge::UDodge()
{
    // Default dodge values - reduced by half
    DodgeSpeed = 1425.0f;
    DodgeDuration = 0.375f;
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
    bWantsToDodge = false;
    DodgeTimer = 0.0f;
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
    
    // Initialize movement component reference
    MovementComponent = nullptr;
}

void UDodge::StartDodge()
{
	UE_LOG(LogTemp, Warning, TEXT("StartDodge called - CanDodge(): %s"), CanDodge() ? TEXT("true") : TEXT("false"));
    
    // Add comprehensive null and validity checks to prevent access violations
    if (!MovementComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("StartDodge: MovementComponent is null!"));
        return;
    }
    
    // Check if the MovementComponent is still valid (not pending kill or destroyed)
    if (!IsValid(MovementComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("StartDodge: MovementComponent is invalid (pending kill or destroyed)!"));
        return;
    }
    
    // Check if we're waiting for server sync after previous dodge
    if (bWaitingForServerSync && MovementComponent->GetPawnOwner()->GetLocalRole() < ROLE_Authority)
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
    if (MovementComponent && MovementComponent->GetPawnOwner()->GetLocalRole() < ROLE_Authority && MovementComponent->bIsDodging && DodgeTimer <= 0.05f && !bWaitingForServerSync)
    {
        // Client can predict immediate dodge if current dodge is finishing very soon and we're not waiting for sync
        bAllowImmediateDodge = true;
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Allowing immediate dodge - current dodge almost finished (%.3f seconds left)"), DodgeTimer);
    }
    
    // Simplified validation - only check if already dodging (unless immediate dodge allowed)
    if (MovementComponent->bIsDodging && !bAllowImmediateDodge)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDodge rejected - already dodging"));
        return;
    }
    
    // Additional check for cooldown to prevent spam (with small tolerance for immediate chaining)
    float CooldownTolerance = bAllowImmediateDodge ? 0.1f : 0.0f;
    if (MovementComponent->DodgeCooldownTimer > CooldownTolerance)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartDodge rejected - still on cooldown: %f (tolerance: %f)"), MovementComponent->DodgeCooldownTimer, CooldownTolerance);
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
        if (MovementComponent && IsValid(MovementComponent))
        {
            APawn* Pawn = MovementComponent->GetPawnOwner();
            if (Pawn && IsValid(Pawn))
            {
                MovementComponent->DodgeDirection = Pawn->GetLastMovementInputVector();
                if (MovementComponent->DodgeDirection.IsZero())
                {
                    // Dodge forward if no input
                    ACharacter* Character = Cast<ACharacter>(Pawn);
                    MovementComponent->DodgeDirection = Character ? Character->GetActorForwardVector() : FVector::ForwardVector;
                }
                MovementComponent->DodgeDirection.Normalize();

                UE_LOG(LogTemp, Warning, TEXT("StartDodge - Setting DodgeDirection: %s"), *MovementComponent->DodgeDirection.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("StartDodge: Pawn is null or invalid!"));
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("StartDodge: MovementComponent is null or invalid during direction calculation!"));
            return;
        }
        
        // If we're a client, send dodge direction to server immediately
        if (MovementComponent && IsValid(MovementComponent))
        {
            APawn* Pawn = MovementComponent->GetPawnOwner();
            if (Pawn && IsValid(Pawn) && Pawn->GetLocalRole() < ROLE_Authority)
            {
                // Only send RPC if we have a valid network connection
                if (Pawn->GetNetConnection() != nullptr)
                {
                    // For bad connections, reduce rate limiting for more responsive chaining
                    float RateLimit = bAllowImmediateDodge ? 0.05f : 0.1f; // Faster rate for chaining
                    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
                    if (CurrentTime - LastDodgeRPCTime >= RateLimit)
                    {
                        // Send dodge request to server with direction
                        MovementComponent->ServerStartDodge(MovementComponent->DodgeDirection);
                        LastDodgeRPCTime = CurrentTime;
                        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Sent ServerStartDodge with direction: %s (rate limit: %f)"), 
                            *MovementComponent->DodgeDirection.ToString(), RateLimit);
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
                    if (MovementComponent && IsValid(MovementComponent) && MovementComponent->bIsDodging)
                    {
                        MovementComponent->bIsDodging = false;
                        MovementComponent->SetMovementMode(MOVE_Walking, 0);
                        DodgeTimer = 0.0f;
                        bClientHasPredictedDodgeEnd = true;
                    }
                    // Reset for new dodge
                    bClientHasPredictedDodgeEnd = false;
                    if (MovementComponent && IsValid(MovementComponent))
                    {
                        MovementComponent->DodgeCooldownTimer = 0.0f; // Allow immediate chaining on client
                    }
                }
            }
        }
        
        bWantsToDodge = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot dodge - bIsDodging: %s, DodgeCooldownTimer: %f, bWantsToDodge: %s"), 
            MovementComponent && MovementComponent->bIsDodging ? TEXT("true") : TEXT("false"),
            MovementComponent ? MovementComponent->DodgeCooldownTimer : 0.0f,
            bWantsToDodge ? TEXT("true") : TEXT("false"));
    }
}

bool UDodge::CanDodge() const
{
	// Add comprehensive null and validity checks to prevent access violations
	if (!MovementComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("CanDodge: MovementComponent is null!"));
		return false;
	}
	
	// Check if the MovementComponent is still valid (not pending kill or destroyed)
	if (!IsValid(MovementComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("CanDodge: MovementComponent is invalid (pending kill or destroyed)!"));
		return false;
	}
	
	// Server has authority over cooldown validation
    if (MovementComponent->GetPawnOwner()->GetLocalRole() == ROLE_Authority)
    {
        // Server-side validation is authoritative
        bool bResult = !MovementComponent->bIsDodging && MovementComponent->DodgeCooldownTimer <= 0.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("CanDodge SERVER check - bIsDodging: %s (replicated), DodgeCooldownTimer: %f (replicated), Result: %s"),
            MovementComponent->bIsDodging ? TEXT("true") : TEXT("false"),
            MovementComponent->DodgeCooldownTimer,
            bResult ? TEXT("true") : TEXT("false"));
            
        return bResult;
    }
    else
    {
        // Client uses replicated values from server but allows for smoother chaining
        bool bBasicResult = !MovementComponent->bIsDodging && MovementComponent->DodgeCooldownTimer <= 0.0f && !bWaitingForServerSync;
        
        // For immediate responsiveness, also allow if current dodge is almost finished (smaller window for more aggressive sync)
        bool bChainResult = MovementComponent->bIsDodging && DodgeTimer <= 0.05f && MovementComponent->DodgeCooldownTimer <= 0.1f && !bWaitingForServerSync;
        
        bool bResult = bBasicResult || bChainResult;
        
        UE_LOG(LogTemp, Warning, TEXT("CanDodge CLIENT check - bIsDodging: %s, DodgeCooldownTimer: %f, DodgeTimer: %f, bWaitingForServerSync: %s, BasicResult: %s, ChainResult: %s, FinalResult: %s"),
            MovementComponent->bIsDodging ? TEXT("true") : TEXT("false"),
            MovementComponent->DodgeCooldownTimer,
            DodgeTimer,
            bWaitingForServerSync ? TEXT("true") : TEXT("false"),
            bBasicResult ? TEXT("true") : TEXT("false"),
            bChainResult ? TEXT("true") : TEXT("false"),
            bResult ? TEXT("true") : TEXT("false"));
            
        return bResult;
    }
}

void UDodge::SetMovementComponent(UMyCharacterMovementComponent* InMovementComponent)
{
	MovementComponent = InMovementComponent;
}

void UDodge::ServerStartDodge(const FVector& Direction)
{
	UE_LOG(LogTemp, Warning, TEXT("ServerStartDodge_Implementation called with Direction: %s"), *Direction.ToString());
    
    // Add comprehensive null and validity checks to prevent access violations
    if (!MovementComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("ServerStartDodge_Implementation: MovementComponent is null!"));
        return;
    }
    
    // Check if the MovementComponent is still valid (not pending kill or destroyed)
    if (!IsValid(MovementComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("ServerStartDodge_Implementation: MovementComponent is invalid (pending kill or destroyed)!"));
        return;
    }
    
    // More lenient server-side validation for smoother chaining (but still aggressive sync)
    bool bAllowChaining = MovementComponent->bIsDodging && DodgeTimer <= 0.05f && MovementComponent->DodgeCooldownTimer <= 0.1f;
    
    // Get PawnOwner for validation
    APawn* PawnOwner = MovementComponent->GetPawnOwner();
    if (!PawnOwner || !IsValid(PawnOwner))
    {
        UE_LOG(LogTemp, Error, TEXT("ServerStartDodge_Implementation: PawnOwner is null or invalid!"));
        return;
    }
    
    // Validate server-side to prevent spam but allow chaining
    if (MovementComponent->bIsDodging && !bAllowChaining)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - already dodging (no chaining allowed)"));
        return;
    }
    
    if (MovementComponent->DodgeCooldownTimer > 0.1f) // Smaller tolerance for aggressive sync
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - still on cooldown: %f"), MovementComponent->DodgeCooldownTimer);
        return;
    }
    
    if (bWantsToDodge && !bAllowChaining)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - already wanting to dodge"));
        return;
    }
    
    // If we're chaining, force end the current dodge first
    if (bAllowChaining && MovementComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: Allowing dodge chaining - ending current dodge first"));
        MovementComponent->bIsDodging = false;
        bWantsToDodge = false;
        MovementComponent->SetMovementMode(MOVE_Walking);
        DodgeTimer = 0.0f;
        MovementComponent->DodgeCooldownTimer = 0.0f; // Reset cooldown for chaining
    }
    
    // Set the dodge direction on server
    MovementComponent->DodgeDirection = Direction;
    
    // Set bWantsToDodge so the server will process the dodge in UpdateCharacterStateBeforeMovement
    if (CanDodge() || bAllowChaining)
    {
        bWantsToDodge = true;
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge set bWantsToDodge=true with direction: %s (chaining: %s)"), 
            *MovementComponent->DodgeDirection.ToString(), bAllowChaining ? TEXT("true") : TEXT("false"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SERVER: ServerStartDodge rejected - CanDodge() returned false"));
    }
}

void UDodge::ClientNotifyDodgeStateChanged_Implementation(bool bNewIsDodging)
{
	UE_LOG(LogTemp, Warning, TEXT("CLIENT: ClientNotifyDodgeStateChanged called - bNewIsDodging: %s, already dodging: %s"),
        bNewIsDodging ? TEXT("true") : TEXT("false"),
        MovementComponent && MovementComponent->bIsDodging ? TEXT("true") : TEXT("false"));
    
    // Add comprehensive null and validity checks to prevent access violations
    if (!MovementComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("ClientNotifyDodgeStateChanged_Implementation: MovementComponent is null!"));
        return;
    }
    
    // Check if the MovementComponent is still valid (not pending kill or destroyed)
    if (!IsValid(MovementComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("ClientNotifyDodgeStateChanged_Implementation: MovementComponent is invalid (pending kill or destroyed)!"));
        return;
    }
    
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
        // Check if client already started dodge via prediction
        if (MovementComponent->bIsDodging)
        {
            // Client already predicted this - server confirmed, just sync timer if needed
            UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server confirmed dodge - client prediction was correct"));
            bClientHasPredictedDodgeEnd = false;
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server confirmed dodge start - starting dodge on client (no prediction)"));

        // Rotate character to face dodge direction before disabling rotation
        RotateToDodgeDirection();
        
        // Disable rotation during dodge
        DisableRotationDuringDodge();
        
        // Initialize position tracking for dodge direction
        APawn* Pawn = MovementComponent->GetPawnOwner();
        if (Pawn && IsValid(Pawn))
        {
            ACharacter* Character = Cast<ACharacter>(Pawn);
            if (Character)
            {
                PreviousDodgePosition = Character->GetActorLocation();
                bHasInitializedDodgePosition = true;
            }
        }
        
        // Start dodge on client now that server has confirmed
        MovementComponent->bIsDodging = true;
        DodgeTimer = DodgeDuration;
        bClientHasPredictedDodgeEnd = false;
        UE_LOG(LogTemp, Warning, TEXT("MovementComponent: %s"), *GetNameSafe(MovementComponent));
        if (MovementComponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("CLIENT: Starting dodge - setting movement mode to CMOVE_Dodge"));
            MovementComponent->SetMovementMode(MOVE_Custom, 0); // 0 corresponds to CMOVE_Dodge
        }
        
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
        MovementComponent->bIsDodging = false;
        bClientHasPredictedDodgeEnd = false;
        if (MovementComponent)
        {
            MovementComponent->SetMovementMode(MOVE_Walking);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Dodge ended after server confirmation"));
    }
}

void UDodge::ClientNotifyCooldownChanged_Implementation(float NewCooldown)
{
	UE_LOG(LogTemp, Warning, TEXT("CLIENT: ClientNotifyCooldownChanged called - NewCooldown: %f"), NewCooldown);
    
    // Add comprehensive null and validity checks to prevent access violations
    if (!MovementComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("ClientNotifyCooldownChanged_Implementation: MovementComponent is null!"));
        return;
    }
    
    // Check if the MovementComponent is still valid (not pending kill or destroyed)
    if (!IsValid(MovementComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("ClientNotifyCooldownChanged_Implementation: MovementComponent is invalid (pending kill or destroyed)!"));
        return;
    }
    
    if (bWaitingForServerSync)
    {
        bWaitingForServerSync = false;
        LastServerSyncTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("CLIENT: Server sync detected via cooldown change - sync complete"));
        
        // More conservative sync window reduction for aggressive sync
        ServerSyncWindow = FMath::Max(ServerSyncWindow * 0.98f, 0.1f);
    }
    
    // Sync cooldown with server's value
    MovementComponent->DodgeCooldownTimer = NewCooldown;
    
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

void UDodge::PlayDodgeMontage()
{
    if (!DodgeMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: DodgeMontage is null!"));
        return;
    }
    
    // Add comprehensive null and validity checks to prevent access violations
    if (!MovementComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayDodgeMontage: MovementComponent is null!"));
        return;
    }
    
    // Check if the MovementComponent is still valid (not pending kill or destroyed)
    if (!IsValid(MovementComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("PlayDodgeMontage: MovementComponent is invalid (pending kill or destroyed)!"));
        return;
    }
    
    APawn* Pawn = MovementComponent->GetPawnOwner();
    if (!Pawn || !IsValid(Pawn))
    {
        UE_LOG(LogTemp, Error, TEXT("PlayDodgeMontage: Pawn is null or invalid!"));
        return;
    }
    
    if (ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        if (USkeletalMeshComponent* MeshComponent = Character->GetMesh())
        {
            if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
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
            UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: MeshComponent is null!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayDodgeMontage: Character cast failed!"));
    }
}

void UDodge::DisableRotationDuringDodge()
{
	// Add comprehensive null and validity checks to prevent access violations
	if (!MovementComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("DisableRotationDuringDodge: MovementComponent is null!"));
		return;
	}
	
	// Check if the MovementComponent is still valid (not pending kill or destroyed)
	if (!IsValid(MovementComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("DisableRotationDuringDodge: MovementComponent is invalid (pending kill or destroyed)!"));
		return;
	}
	
	 // Save current rotation settings
    bSavedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
    bSavedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
    SavedRotationRate = MovementComponent->RotationRate;
    
    // Disable all rotation
    MovementComponent->bOrientRotationToMovement = false;
    MovementComponent->bUseControllerDesiredRotation = false;
    MovementComponent->RotationRate = FRotator::ZeroRotator;
    
    UE_LOG(LogTemp, Warning, TEXT("DisableRotationDuringDodge: Rotation disabled"));
}

void UDodge::RestoreRotationAfterDodge()
{
	// Add comprehensive null and validity checks to prevent access violations
	if (!MovementComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("RestoreRotationAfterDodge: MovementComponent is null!"));
		return;
	}
	
	// Check if the MovementComponent is still valid (not pending kill or destroyed)
	if (!IsValid(MovementComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("RestoreRotationAfterDodge: MovementComponent is invalid (pending kill or destroyed)!"));
		return;
	}
	
	// Restore saved rotation settings
    MovementComponent->bOrientRotationToMovement = bSavedOrientRotationToMovement;
    MovementComponent->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
    MovementComponent->RotationRate = SavedRotationRate;
    
    UE_LOG(LogTemp, Warning, TEXT("RestoreRotationAfterDodge: Rotation restored (OrientToMovement: %s, UseControllerDesired: %s, Rate: %s)"), 
        MovementComponent->bOrientRotationToMovement ? TEXT("true") : TEXT("false"),
        MovementComponent->bUseControllerDesiredRotation ? TEXT("true") : TEXT("false"),
        *MovementComponent->RotationRate.ToString());
}

void UDodge::RotateToDodgeDirection()
{
	// Add comprehensive null and validity checks to prevent access violations
	if (!MovementComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("RotateToDodgeDirection: MovementComponent is null!"));
		return;
	}
	
	// Check if the MovementComponent is still valid (not pending kill or destroyed)
	if (!IsValid(MovementComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("RotateToDodgeDirection: MovementComponent is invalid (pending kill or destroyed)!"));
		return;
	}
	
	if (!MovementComponent->DodgeDirection.IsZero())
    {
        APawn* Pawn = MovementComponent->GetPawnOwner();
        if (!Pawn || !IsValid(Pawn))
        {
            UE_LOG(LogTemp, Error, TEXT("RotateToDodgeDirection: Pawn is null or invalid!"));
            return;
        }
        
        ACharacter* Character = Cast<ACharacter>(Pawn);
        if (Character)
        {
            // Calculate rotation to face dodge direction (ignore Z component for yaw-only rotation)
            FVector FlatDodgeDirection = MovementComponent->DodgeDirection;
            FlatDodgeDirection.Z = 0.0f;
            FlatDodgeDirection.Normalize();
            
            if (!FlatDodgeDirection.IsZero())
            {
                // Create rotation from direction vector
                FRotator TargetRotation = FlatDodgeDirection.Rotation();
                
                // Instantly set the character's rotation to face dodge direction
                Character->SetActorRotation(TargetRotation);
                
                UE_LOG(LogTemp, Warning, TEXT("RotateToDodgeDirection: Rotated character to %s (dodge direction: %s)"), 
                    *TargetRotation.ToString(), *MovementComponent->DodgeDirection.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("RotateToDodgeDirection: Character cast failed!"));
        }
    }
}

void UDodge::UpdateRotationBasedOnMovement(float DeltaTime)
{
	// Add comprehensive null and validity checks to prevent access violations
	if (!MovementComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateRotationBasedOnMovement: MovementComponent is null!"));
		return;
	}
	
	// Check if the MovementComponent is still valid (not pending kill or destroyed)
	if (!IsValid(MovementComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateRotationBasedOnMovement: MovementComponent is invalid (pending kill or destroyed)!"));
		return;
	}
	
	if (!MovementComponent->bIsDodging)
    {
        return;
    }
    
    APawn* Pawn = MovementComponent->GetPawnOwner();
    if (!Pawn || !IsValid(Pawn))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateRotationBasedOnMovement: Pawn is null or invalid!"));
        return;
    }
    
    ACharacter* Character = Cast<ACharacter>(Pawn);
    if (!Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateRotationBasedOnMovement: Character cast failed!"));
        return;
    }
    
    // Get current position
    FVector CurrentPosition = Character->GetActorLocation();
    
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
            FRotator CurrentRotation = Character->GetActorRotation();
            FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
            
            Character->SetActorRotation(NewRotation);
            
            UE_LOG(LogTemp, Verbose, TEXT("UpdateRotationBasedOnMovement: Movement delta: %s, Target rotation: %s"), 
                *MovementDelta.ToString(), *TargetRotation.ToString());
        }
    }
    
    // Store current position for next frame
    PreviousDodgePosition = CurrentPosition;
}

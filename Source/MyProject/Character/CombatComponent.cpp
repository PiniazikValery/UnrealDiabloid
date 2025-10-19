// Physics-based dodge + attack component implementation
#include "CombatComponent.h"
#include "../MyProjectCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AMyProjectCharacter>(GetOwner());
}

// ============== COMMENTED OUT: Original Velocity-Based Dodge Implementation ==============
/*
void UCombatComponent::StartDodge()
{
	UE_LOG(LogTemp, Warning, TEXT("===== StartDodge CALLED ====="));
	
	if (!CanDodge() || !OwnerCharacter.IsValid()) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] StartDodge: Failed CanDodge=%s, ValidOwner=%s"), 
			OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
			CanDodge() ? TEXT("true") : TEXT("false"),
			OwnerCharacter.IsValid() ? TEXT("true") : TEXT("false"));
		return;
	}
	
	FVector DodgeDir = GetLookDirection();
	UE_LOG(LogTemp, Warning, TEXT("[%s] StartDodge: Direction=(%f,%f,%f), HasAuthority=%s, IsLocallyControlled=%s"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		DodgeDir.X, DodgeDir.Y, DodgeDir.Z,
		OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
		OwnerCharacter->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	
	// Server executes immediately
	if (OwnerCharacter->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SERVER] Executing dodge with authority"));
		ExecuteDodge(DodgeDir);
		return;
	}
	
	// Client: Only execute if locally controlled (prediction)
	if (OwnerCharacter->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Executing local prediction and sending RPC"));
		
		// Execute locally for immediate feedback (prediction)
		ExecuteDodge(DodgeDir);
		
		// Send validated direction to server
		ServerStartDodge(DodgeDir);
	}
}
*/

// ============== NEW: Impulse-Based Dodge Implementation ==============
void UCombatComponent::StartDodge()
{
	UE_LOG(LogTemp, Warning, TEXT("===== StartDodge CALLED (IMPULSE-BASED) ====="));
	
	if (!CanDodge() || !OwnerCharacter.IsValid()) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] StartDodge: Failed CanDodge=%s, ValidOwner=%s"), 
			OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
			CanDodge() ? TEXT("true") : TEXT("false"),
			OwnerCharacter.IsValid() ? TEXT("true") : TEXT("false"));
		return;
	}
	
	FVector DodgeDir = GetLookDirection();
	UE_LOG(LogTemp, Warning, TEXT("[%s] StartDodge: Direction=(%f,%f,%f), HasAuthority=%s, IsLocallyControlled=%s"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		DodgeDir.X, DodgeDir.Y, DodgeDir.Z,
		OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
		OwnerCharacter->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	
	// Server executes immediately
	if (OwnerCharacter->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SERVER] Executing impulse dodge with authority"));
		ExecuteImpulseDodge(DodgeDir);
		return;
	}
	
	// Client: Only execute if locally controlled (prediction)
	if (OwnerCharacter->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Executing local impulse prediction and sending RPC"));
		
		// Execute locally for immediate feedback (prediction)
		ExecuteImpulseDodge(DodgeDir);
		
		// Send validated direction to server
		ServerStartDodge(DodgeDir);
	}
}

void UCombatComponent::ServerStartDodge_Implementation(FVector DodgeDirection)
{
	UE_LOG(LogTemp, Warning, TEXT("[SERVER] ServerStartDodge_Implementation: Direction=(%f,%f,%f), CanDodge=%s"), 
		DodgeDirection.X, DodgeDirection.Y, DodgeDirection.Z,
		CanDodge() ? TEXT("true") : TEXT("false"));
	
	// Server validation
	if (!CanDodge()) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[SERVER] ServerStartDodge rejected: CanDodge failed"));
		return;
	}
	
	// Validate direction magnitude (prevent cheating)
	FVector NormalizedDirection = DodgeDirection.GetSafeNormal();
	if (NormalizedDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SERVER] ServerStartDodge rejected: Invalid direction"));
		return;
	}
	
	// Additional server-side validation
	if (!OwnerCharacter.IsValid() || bIsDodging)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SERVER] ServerStartDodge rejected: Invalid character or already dodging"));
		return;
	}
	
	// Execute impulse-based dodge on server - this will replicate to all clients
	ExecuteImpulseDodge(NormalizedDirection);
}

void UCombatComponent::ServerEndDodge_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[SERVER] ServerEndDodge_Implementation: bIsDodging=%s"), 
		bIsDodging ? TEXT("true") : TEXT("false"));
	
	// Server validation - only allow ending if currently dodging
	if (!bIsDodging || !OwnerCharacter.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SERVER] ServerEndDodge rejected: Not dodging or invalid character"));
		return;
	}
	
	// Execute on server - this will replicate to all clients
	EndDodge();
}

void UCombatComponent::OnRep_DodgeDirection()
{
	UE_LOG(LogTemp, Warning, TEXT("[CLIENT] OnRep_DodgeDirection: Direction=(%f,%f,%f), bIsDodging=%s, NetworkQuality=%f"), 
		ReplicatedDodgeDirection.X, ReplicatedDodgeDirection.Y, ReplicatedDodgeDirection.Z,
		bIsDodging ? TEXT("true") : TEXT("false"),
		NetworkQualityScore);
	
	// Only apply corrections for non-locally controlled characters
	if (!OwnerCharacter.IsValid() || OwnerCharacter->IsLocallyControlled()) return;
	
	// Store last known position for interpolation
	LastReplicatedPosition = OwnerCharacter->GetActorLocation();
	
	// Predict end position for smoother interpolation
	if (bIsDodging && !ReplicatedDodgeDirection.IsNearlyZero())
	{
		PredictedEndPosition = LastReplicatedPosition + (ReplicatedDodgeDirection * DodgeDistance);
		
		// For poor connections, pre-calculate the entire path
		if (NetworkQualityScore < PoorConnectionThreshold)
		{
			PreCalculateDodgePath();
		}
		
		// Use impulse-based execution for replication
		ExecuteImpulseDodge(ReplicatedDodgeDirection);
	}
}

void UCombatComponent::OnRep_DodgeStartTime()
{
	UE_LOG(LogTemp, Warning, TEXT("[CLIENT] OnRep_DodgeStartTime: StartTime=%f, CurrentTime=%f"), 
		DodgeStartTime, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
	
	// Only apply corrections for non-locally controlled characters
	if (!OwnerCharacter.IsValid() || OwnerCharacter->IsLocallyControlled()) return;
	
	// Update our local timing based on server time
	if (bIsDodging && GetWorld())
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		DodgeEndTime = DodgeStartTime + DodgeDuration;
		InvincibilityEndTime = DodgeStartTime + DodgeInvincibilityDuration;
		NextDodgeTime = DodgeStartTime + DodgeDuration + DodgeCooldown;
		
		// Check if dodge should have already ended
		if (CurrentTime >= DodgeEndTime)
		{
			EndDodge();
		}
	}
}

FVector UCombatComponent::GetLookDirection() const
{
	if (!OwnerCharacter.IsValid()) return FVector::ForwardVector;
	
	// For dodge direction, we want to use input direction relative to camera
	// Check if we have current input movement
	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		FVector LastInput = MoveComp->GetLastInputVector();
		UE_LOG(LogTemp, Warning, TEXT("Last Input Vector: X=%f, Y=%f, Z=%f"), 
			LastInput.X, LastInput.Y, LastInput.Z);
		
		if (!LastInput.IsNearlyZero())
		{
			// Input is already in world space relative to character
			return LastInput.GetSafeNormal();
		}
	}
	
	// For Diablo-style games, use character's current facing direction when standing still
	// This represents where the character/player is oriented
	FVector CharacterForward = OwnerCharacter->GetActorForwardVector();
	UE_LOG(LogTemp, Warning, TEXT("Using Character Forward: X=%f, Y=%f, Z=%f"), 
		CharacterForward.X, CharacterForward.Y, CharacterForward.Z);
	
	return CharacterForward.GetSafeNormal();
}

FVector UCombatComponent::CalculateDodgeDirection() const
{
	if (!OwnerCharacter.IsValid()) return FVector::ForwardVector;
	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	FVector InputDir = FVector::ZeroVector;
	if (MoveComp)
	{
		FVector LastInput = MoveComp->GetLastInputVector();
		if (!LastInput.IsNearlyZero())
		{
			AController* Controller = OwnerCharacter->GetController();
			if (Controller)
			{
				FRotator ControlRot = Controller->GetControlRotation();
				FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
				FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
				FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
				InputDir = (Forward * LastInput.X + Right * LastInput.Y).GetSafeNormal();
			}
		}
	}
	if (InputDir.IsNearlyZero())
	{
		InputDir = -OwnerCharacter->GetActorForwardVector().GetSafeNormal(); // backstep
	}
	return InputDir.GetSafeNormal();
}

// ============== COMMENTED OUT: Original Velocity-Controlled Dodge Execution ==============
/*
void UCombatComponent::ExecuteDodge(FVector Direction)
{
	if (!OwnerCharacter.IsValid()) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[DODGE] ExecuteDodge: Invalid OwnerCharacter"));
		return;
	}
	
	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[DODGE] ExecuteDodge: No MovementComponent"));
		return;
	}

	// CRITICAL: Use the passed Direction parameter, don't recalculate
	FVector FinalDirection = Direction.GetSafeNormal();
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] ExecuteDodge: Using passed Direction=(%f,%f,%f), DodgeDistance=%f, DodgeDuration=%f"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		FinalDirection.X, FinalDirection.Y, FinalDirection.Z, DodgeDistance, DodgeDuration);

	bIsDodging = true;
	bIsInvincible = true;
	ReplicatedDodgeDirection = FinalDirection;
	DodgeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	DodgeEndTime = DodgeStartTime + DodgeDuration;
	InvincibilityEndTime = DodgeStartTime + DodgeInvincibilityDuration;
	NextDodgeTime = DodgeStartTime + DodgeDuration + DodgeCooldown;

	UE_LOG(LogTemp, Warning, TEXT("[%s] ExecuteDodge: Times - Start=%f, End=%f, NextAllowed=%f"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		DodgeStartTime, DodgeEndTime, NextDodgeTime);

	// Store original movement settings
	OriginalGroundFriction = MoveComp->GroundFriction;
	OriginalBrakingDeceleration = MoveComp->BrakingDecelerationWalking;
	
	// Temporarily modify movement settings for dodge
	MoveComp->GroundFriction = 0.f;
	MoveComp->BrakingDecelerationWalking = 0.f;
	MoveComp->bUseSeparateBrakingFriction = false;

	// Calculate dodge speed and velocity
	float DodgeSpeed = (DodgeDuration > 0.f) ? (DodgeDistance / DodgeDuration) : 800.f;
	
	// Clamp dodge speed to reasonable limits to prevent extreme movement
	DodgeSpeed = FMath::Clamp(DodgeSpeed, MinDodgeSpeed, MaxDodgeSpeed);
	
	DodgeVelocity = FinalDirection * DodgeSpeed;

	UE_LOG(LogTemp, Warning, TEXT("[%s] ExecuteDodge: DodgeSpeed=%f (clamped), DodgeVelocity=(%f,%f,%f)"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		DodgeSpeed, DodgeVelocity.X, DodgeVelocity.Y, DodgeVelocity.Z);
	
	// Since UpdateDodge is disabled, use full speed immediately for better feel
	FVector InitialVelocity = FinalDirection * DodgeSpeed;
	InitialVelocity.Z = MoveComp->Velocity.Z; // Preserve vertical velocity (gravity, etc.)
	
	// Set the full dodge velocity immediately
	MoveComp->Velocity = InitialVelocity;

	// Set character rotation to face dodge direction
	if (!FinalDirection.IsNearlyZero())
	{
		FRotator FaceRot = FinalDirection.Rotation();
		OwnerCharacter->SetActorRotation(FRotator(0.f, FaceRot.Yaw, 0.f));
	}

	// Cancel any ongoing attack
	if (bIsAttacking)
	{
		bIsAttacking = false;
		bIsAttackEnding = false;
		bIsSecondAttackWindowOpen = false;
	}
	
	// Disable auto-rotation during dodge
	MoveComp->bOrientRotationToMovement = true;
	
	// Set up timer to ensure dodge ends properly (since UpdateDodge is commented out)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DodgeEndTimerHandle,
			this,
			&UCombatComponent::EndDodge,
			DodgeDuration,
			false
		);
	}
}
*/

// ============== NEW: Impulse-Based Dodge Execution ==============
void UCombatComponent::ExecuteImpulseDodge(FVector Direction)
{
	if (!OwnerCharacter.IsValid()) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMPULSE DODGE] ExecuteImpulseDodge: Invalid OwnerCharacter"));
		return;
	}
	
	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMPULSE DODGE] ExecuteImpulseDodge: No MovementComponent"));
		return;
	}

	// Use the passed Direction parameter
	FVector FinalDirection = Direction.GetSafeNormal();
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] ExecuteImpulseDodge: Direction=(%f,%f,%f), DodgeDistance=%f"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		FinalDirection.X, FinalDirection.Y, FinalDirection.Z, DodgeDistance);

	// Set dodge state
	bIsDodging = true;
	// bIsInvincible = true;
	// ReplicatedDodgeDirection = FinalDirection;
	// DodgeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// DodgeEndTime = DodgeStartTime + DodgeDuration;
	// InvincibilityEndTime = DodgeStartTime + DodgeInvincibilityDuration;
	// NextDodgeTime = DodgeStartTime + DodgeDuration + DodgeCooldown;

	// UE_LOG(LogTemp, Warning, TEXT("[%s] ExecuteImpulseDodge: Times - Start=%f, End=%f, NextAllowed=%f"), 
	// 	OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
	// 	DodgeStartTime, DodgeEndTime, NextDodgeTime);

	// // Store original movement settings for restoration later
	// OriginalGroundFriction = MoveComp->GroundFriction;
	// OriginalBrakingDeceleration = MoveComp->BrakingDecelerationWalking;
	
	// Simple hardcoded impulse approach
	FVector HardcodedImpulse = FinalDirection * 1000.f; // Hardcoded strength
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] ExecuteImpulseDodge: Applying hardcoded impulse=(%f,%f,%f)"), 
		OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		HardcodedImpulse.X, HardcodedImpulse.Y, HardcodedImpulse.Z);

	// Apply the impulse - this gives instant velocity change
	MoveComp->AddImpulse(HardcodedImpulse, true);
	
	// Store a reference velocity for debugging (simplified)
	// DodgeVelocity = FinalDirection * 1000.f; // Hardcoded reference speed

	// Set character rotation to face dodge direction
	// if (!FinalDirection.IsNearlyZero())
	// {
	// 	FRotator FaceRot = FinalDirection.Rotation();
	// 	OwnerCharacter->SetActorRotation(FRotator(0.f, FaceRot.Yaw, 0.f));
	// }

	// Cancel any ongoing attack
	// if (bIsAttacking)
	// {
	// 	bIsAttacking = false;
	// 	bIsAttackEnding = false;
	// 	bIsSecondAttackWindowOpen = false;
	// }
	
	// Set up timer to end dodge after duration
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DodgeEndTimerHandle,
			this,
			&UCombatComponent::EndDodge,
			DodgeDuration,
			false
		);
	}
}

float UCombatComponent::CalculateSmoothDodgeProgress(float Progress) const
{
	// Clamp progress to [0,1]
	Progress = FMath::Clamp(Progress, 0.f, 1.f);
	
	// Use smooth step function for better easing
	// SmoothStep(x) = 3x² - 2x³ (provides smooth acceleration and deceleration)
	return Progress * Progress * (3.f - 2.f * Progress);
}

float UCombatComponent::CalculateSpeedMultiplier(float Progress) const
{
	Progress = FMath::Clamp(Progress, 0.f, 1.f);
	
	// Use adaptive acceleration time based on connection quality
	float AdaptiveAccelTime = GetAdaptiveAccelerationTime();
	
	// Calculate acceleration phase (0 to AdaptiveAccelTime)
	float AccelPhase = AdaptiveAccelTime / DodgeDuration;
	// Calculate deceleration phase (DodgeDuration - DodgeDecelerationTime to DodgeDuration)
	float DecelPhase = 1.f - (DodgeDecelerationTime / DodgeDuration);
	
	float SpeedMultiplier;
	
	if (Progress <= AccelPhase)
	{
		// Acceleration phase: smooth ease-in from 0 to 1
		float AccelProgress = Progress / AccelPhase;
		float SmoothAccel = CalculateSmoothDodgeProgress(AccelProgress);
		SpeedMultiplier = SmoothAccel;
	}
	else if (Progress >= DecelPhase)
	{
		// Deceleration phase: smooth ease-out from 1 to 0
		float DecelProgress = (Progress - DecelPhase) / (1.f - DecelPhase);
		float SmoothDecel = 1.f - CalculateSmoothDodgeProgress(DecelProgress);
		SpeedMultiplier = SmoothDecel;
	}
	else
	{
		// Constant speed phase
		SpeedMultiplier = 1.f;
	}
	
	// Apply additional curve smoothing (less aggressive for good connections)
	float CurveExponent = IsGoodConnection() ? (SmoothingCurveExponent * 0.5f) : SmoothingCurveExponent;
	SpeedMultiplier = FMath::Pow(SpeedMultiplier, 1.f / CurveExponent);
	
	return FMath::Clamp(SpeedMultiplier, 0.f, 1.f);
}

bool UCombatComponent::IsGoodConnection() const
{
	if (!bAdaptiveSmoothing || !OwnerCharacter.IsValid())
	{
		return false; // Default to smooth mode if adaptive is disabled
	}
	
	// Server or single player always has "good" connection
	if (OwnerCharacter->HasAuthority())
	{
		return true;
	}
	
	// For clients, check if this is locally controlled (good responsive feel)
	// and use a simple heuristic: locally controlled = good connection feel needed
	if (OwnerCharacter->IsLocallyControlled())
	{
		return true;
	}
	
	// Non-locally controlled clients (simulated proxies) use smooth mode
	return false;
}

float UCombatComponent::GetAdaptiveInterpSpeed() const
{
	return IsGoodConnection() ? FastVelocityInterpSpeed : VelocityInterpSpeed;
}

float UCombatComponent::GetAdaptiveAccelerationTime() const
{
	return IsGoodConnection() ? FastAccelerationTime : DodgeAccelerationTime;
}

void UCombatComponent::UpdateDodgeVelocity(float DeltaTime, float CurrentTime, UCharacterMovementComponent* MoveComp)
{
	// Early return for invalid states
	if (!MoveComp || ReplicatedDodgeDirection.IsNearlyZero()) return;
	
	// Calculate progress through the dodge duration (cached calculation)
	float ElapsedTime = CurrentTime - DodgeStartTime;
	float Progress = FMath::Clamp(ElapsedTime / DodgeDuration, 0.f, 1.f);
	
	// Get smooth speed multiplier (expensive calculation, but necessary)
	float SpeedMultiplier = CalculateSpeedMultiplier(Progress);
	
	// Calculate target speed and velocity (optimized)
	float BaseSpeed = DodgeDistance / DodgeDuration;
	float CurrentSpeed = FMath::Clamp(BaseSpeed * SpeedMultiplier, 0.f, MaxDodgeSpeed);
	
	// Cache Z velocity to avoid multiple accesses
	float OriginalZ = MoveComp->Velocity.Z;
	FVector TargetVelocity = ReplicatedDodgeDirection * CurrentSpeed;
	TargetVelocity.Z = OriginalZ;
	
	// Adaptive interpolation based on network quality (cached value)
	float AdaptiveInterpSpeed = GetAdaptiveInterpSpeed();
	
	// For poor connections, use extra smoothing (avoid repeated threshold checks)
	if (NetworkQualityScore < PoorConnectionThreshold)
	{
		AdaptiveInterpSpeed *= (0.3f + NetworkQualityScore * 0.7f);
	}
	
	// Optimized velocity interpolation (avoid creating temporary vectors)
	FVector& CurrentVelocity = MoveComp->Velocity;
	FVector HorizontalTarget(TargetVelocity.X, TargetVelocity.Y, 0.f);
	FVector HorizontalCurrent(CurrentVelocity.X, CurrentVelocity.Y, 0.f);
	
	FVector SmoothedVelocity = FMath::VInterpTo(HorizontalCurrent, HorizontalTarget, DeltaTime, AdaptiveInterpSpeed);
	
	// Apply velocity prediction for poor connections (expensive, only when needed)
	if (NetworkQualityScore < PoorConnectionThreshold && bUsePositionExtrapolation)
	{
		FVector PredictedVelocity = PredictFutureVelocity(Progress, DeltaTime);
		float ExtrapolationWeight = ExtrapolationStrength * (1.0f - NetworkQualityScore);
		SmoothedVelocity = FMath::Lerp(SmoothedVelocity, PredictedVelocity, ExtrapolationWeight);
	}
	
	// Apply final velocity (direct assignment)
	CurrentVelocity.X = SmoothedVelocity.X;
	CurrentVelocity.Y = SmoothedVelocity.Y;
	// Z is already preserved from OriginalZ
}

void UCombatComponent::ApplyNetworkSmoothing(float DeltaTime, float CurrentTime, UCharacterMovementComponent* MoveComp)
{
	// Early return for invalid states
	if (!MoveComp || ReplicatedDodgeDirection.IsNearlyZero()) return;
	
	// Cache current position
	FVector CurrentPosition = OwnerCharacter->GetActorLocation();
	
	// Update position history (throttled internally)
	UpdatePositionHistory(CurrentTime, CurrentPosition);
	
	// Calculate expected position based on dodge progress (optimized)
	float ElapsedTime = CurrentTime - DodgeStartTime;
	float Progress = FMath::Clamp(ElapsedTime / DodgeDuration, 0.f, 1.f);
	
	// Smooth progress using easing function (cached result)
	float SmoothedProgress = CalculateSmoothDodgeProgress(Progress);
	
	// Calculate where we should be (avoid repeated vector operations)
	FVector StartPos = LastReplicatedPosition;
	if (PositionHistory.Num() > 0)
	{
		StartPos = PositionHistory[0].Value;
	}
	
	FVector ExpectedCurrentPos = StartPos + (ReplicatedDodgeDirection * (DodgeDistance * SmoothedProgress));
	
	// Smooth current position towards expected position (cached smoothing speed)
	float SmoothingSpeed = GetNetworkSmoothingSpeed();
	FVector SmoothedPosition = FMath::VInterpTo(CurrentPosition, ExpectedCurrentPos, DeltaTime, SmoothingSpeed);
	
	// Apply the smoothed position
	OwnerCharacter->SetActorLocation(SmoothedPosition, true);
	
	// Update velocity to match movement (optimized calculation)
	FVector PositionDelta = SmoothedPosition - CurrentPosition;
	if (DeltaTime > KINDA_SMALL_NUMBER) // Use engine constant for better performance
	{
		float InvDeltaTime = 1.0f / DeltaTime; // Avoid division in vector math
		FVector ImpliedVelocity = PositionDelta * InvDeltaTime;
		ImpliedVelocity.Z = MoveComp->Velocity.Z; // Preserve vertical
		MoveComp->Velocity = ImpliedVelocity;
	}
}

void UCombatComponent::ApplyClientSideCorrection(float DeltaTime, float CurrentTime, UCharacterMovementComponent* MoveComp)
{
	// Early returns for performance
	if (!MoveComp || OwnerCharacter->HasAuthority() || ReplicatedDodgeDirection.IsNearlyZero()) return;
	
	// Throttle corrections to reduce per-frame cost
	static float LastCorrectionTime = 0.f;
	const float CorrectionInterval = 0.1f; // Only check every 100ms
	if (CurrentTime - LastCorrectionTime < CorrectionInterval) return;
	LastCorrectionTime = CurrentTime;
	
	// Calculate progress and expected position (optimized)
	float ElapsedTime = CurrentTime - DodgeStartTime;
	float Progress = FMath::Clamp(ElapsedTime / DodgeDuration, 0.f, 1.f);
	
	FVector ExpectedPosition = LastReplicatedPosition + (ReplicatedDodgeDirection * (DodgeDistance * Progress));
	FVector CurrentPosition = OwnerCharacter->GetActorLocation();
	
	// Check for significant position desync (cached calculation)
	float PositionErrorSq = FVector::DistSquared(CurrentPosition, ExpectedPosition);
	const float MaxAllowedErrorSq = 2500.f; // 50^2 units squared (avoid sqrt)
	
	if (PositionErrorSq > MaxAllowedErrorSq)
	{
		// Apply gentle correction to prevent teleporting (optimized calculation)
		float PositionError = FMath::Sqrt(PositionErrorSq); // Only sqrt when needed
		float CorrectionStrength = FMath::Clamp(PositionError * 0.005f, 0.1f, 0.5f); // Pre-calculated 1/200
		
		FVector CorrectedPosition = FMath::VInterpTo(CurrentPosition, ExpectedPosition, 
			DeltaTime, CorrectionStrength * 2.f);
		
		OwnerCharacter->SetActorLocation(CorrectedPosition, true);
		
		// Update velocity to match the correction (avoid division)
		if (DeltaTime > KINDA_SMALL_NUMBER)
		{
			FVector CorrectionVelocity = (CorrectedPosition - CurrentPosition) / DeltaTime;
			CorrectionVelocity.Z = MoveComp->Velocity.Z; // Preserve vertical velocity
			
			// Use faster interpolation for corrections
			MoveComp->Velocity = FMath::VInterpTo(MoveComp->Velocity, CorrectionVelocity, DeltaTime, 10.f);
		}
	}
}

FVector UCombatComponent::PredictFutureVelocity(float CurrentProgress, float DeltaTime)
{
	// Predict where we should be going based on recent history
	if (PositionHistory.Num() < 2)
	{
		return ReplicatedDodgeDirection * (DodgeDistance / DodgeDuration);
	}
	
	// Calculate velocity trend from position history
	FVector VelocityTrend = FVector::ZeroVector;
	int32 SampleCount = FMath::Min(3, PositionHistory.Num() - 1);
	
	for (int32 i = 0; i < SampleCount; ++i)
	{
		FVector PosDelta = PositionHistory[i].Value - PositionHistory[i + 1].Value;
		float TimeDelta = PositionHistory[i].Key - PositionHistory[i + 1].Key;
		
		if (TimeDelta > 0.f)
		{
			VelocityTrend += (PosDelta / TimeDelta);
		}
	}
	
	if (SampleCount > 0)
	{
		VelocityTrend /= SampleCount;
	}
	
	// Blend between predicted trend and expected dodge velocity
	float SpeedMultiplier = CalculateSpeedMultiplier(CurrentProgress);
	FVector ExpectedVelocity = ReplicatedDodgeDirection * (DodgeDistance / DodgeDuration) * SpeedMultiplier;
	
	return FMath::Lerp(ExpectedVelocity, VelocityTrend, 0.3f); // 30% weight to trend
}

void UCombatComponent::UpdatePositionHistory(float Time, const FVector& Position)
{
	// Throttle position history updates to reduce per-frame cost
	static float LastUpdateTime = 0.f;
	const float UpdateInterval = 0.033f; // ~30Hz updates instead of every frame
	
	if (Time - LastUpdateTime < UpdateInterval)
	{
		return; // Skip this update to reduce performance cost
	}
	LastUpdateTime = Time;
	
	// Add new position to history (optimized insertion)
	PositionHistory.Insert(TPair<float, FVector>(Time, Position), 0);
	
	// Remove old entries (only when necessary)
	if (PositionHistory.Num() > MaxHistorySize)
	{
		PositionHistory.SetNum(MaxHistorySize); // More efficient than RemoveAt
	}
	
	// Update network quality score based on replication frequency (throttled)
	if (LastReplicationTime > 0.f)
	{
		float ReplicationDelta = Time - LastReplicationTime;
		
		// Use faster interpolation for network quality to avoid lag
		AverageReplicationDelta = FMath::Lerp(AverageReplicationDelta, ReplicationDelta, 0.2f);
		
		// Calculate quality score (expecting ~60Hz updates for good connection)
		const float ExpectedDelta = 1.0f / 60.0f;
		NetworkQualityScore = FMath::Clamp(ExpectedDelta / FMath::Max(AverageReplicationDelta, 0.001f), 0.f, 1.f);
	}
	
	LastReplicationTime = Time;
}

float UCombatComponent::GetNetworkSmoothingSpeed() const
{
	// Adjust smoothing speed based on network quality
	float BaseSpeed = 10.f;
	
	if (NetworkQualityScore < PoorConnectionThreshold)
	{
		// Slower interpolation for poor connections
		return BaseSpeed * (0.3f + NetworkQualityScore * 0.7f);
	}
	
	return BaseSpeed;
}

void UCombatComponent::PreCalculateDodgePath()
{
	// Pre-calculate key points along the dodge path for smoother interpolation
	// This helps with poor connections by having reference points ready
	
	if (!OwnerCharacter.IsValid()) return;
	
	FVector StartPos = OwnerCharacter->GetActorLocation();
	FVector EndPos = StartPos + (ReplicatedDodgeDirection * DodgeDistance);
	
	// Store key waypoints for the dodge (helps with laggy interpolation)
	LastReplicatedPosition = StartPos;
	PredictedEndPosition = EndPos;
	
	UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] Pre-calculated dodge path from (%f,%f,%f) to (%f,%f,%f)"),
		StartPos.X, StartPos.Y, StartPos.Z,
		EndPos.X, EndPos.Y, EndPos.Z);
}

void UCombatComponent::UpdateDodge(float DeltaTime)
{
	if (!bIsDodging || !OwnerCharacter.IsValid()) return;
	
	// Cache world reference once per frame
	UWorld* World = GetWorld();
	if (!World) return;
	
	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp) return;

	float Time = World->GetTimeSeconds();
	
	// Update invincibility (lightweight check)
	if (bIsInvincible && Time >= InvincibilityEndTime)
	{
		bIsInvincible = false;
	}

	// Check if dodge should end with client-side lag tolerance
	float EndTimeWithTolerance = DodgeEndTime;
	if (!OwnerCharacter->HasAuthority())
	{
		EndTimeWithTolerance += 0.1f; // 100ms tolerance
	}
	
	if (Time >= EndTimeWithTolerance)
	{
		// Follow the same pattern as StartDodge for network consistency
		if (OwnerCharacter->HasAuthority())
		{
			// Server: Execute immediately
			EndDodge();
		}
		else if (OwnerCharacter->IsLocallyControlled())
		{
			// Locally controlled client: Execute prediction and send RPC
			EndDodge();
			ServerEndDodge();
		}
		// Non-locally controlled clients: Wait for server replication
		return;
	}

	// Network-aware velocity updates following the same pattern as StartDodge
	if (OwnerCharacter->HasAuthority())
	{
		// Server: Always update velocity authoritatively
		UpdateDodgeVelocity(DeltaTime, Time, MoveComp);
	}
	else if (OwnerCharacter->IsLocallyControlled())
	{
		// Locally controlled client: Update for immediate feedback (prediction)
		UpdateDodgeVelocity(DeltaTime, Time, MoveComp);
	}
	else
	{
		// Non-locally controlled clients (simulated proxies): Use smooth interpolation
		ApplyNetworkSmoothing(DeltaTime, Time, MoveComp);
		
		// Additional client-side correction for position desync (only for poor connections)
		if (NetworkQualityScore < PoorConnectionThreshold)
		{
			ApplyClientSideCorrection(DeltaTime, Time, MoveComp);
		}
	}
}

void UCombatComponent::EndDodge()
{
// 	if (!OwnerCharacter.IsValid()) return;
	
// 	// Cache frequently used values
// 	UWorld* World = GetWorld();
// 	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	
	bIsDodging = false;
// 	bIsInvincible = false;
	
// 	if (MoveComp)
// 	{
// 		// Restore movement settings
// 		MoveComp->GroundFriction = OriginalGroundFriction;
// 		MoveComp->BrakingDecelerationWalking = OriginalBrakingDeceleration;
// 		MoveComp->bUseSeparateBrakingFriction = true;
// 		MoveComp->bOrientRotationToMovement = true;
		
// 		// Network-aware velocity handling following the same pattern as StartDodge
// 		if (OwnerCharacter->HasAuthority())
// 		{
// 			// Server: Apply velocity reduction authoritatively
// 			const float VelocityReductionFactor = (NetworkQualityScore < PoorConnectionThreshold) ? 0.5f : 0.3f;
// 			FVector& Velocity = MoveComp->Velocity;
// 			const float OriginalZ = Velocity.Z;
// 			Velocity *= VelocityReductionFactor;
// 			Velocity.Z = OriginalZ;
// 		}
// 		else if (OwnerCharacter->IsLocallyControlled())
// 		{
// 			// Locally controlled client: Apply smooth velocity reduction for immediate feedback (prediction)
// 			const float VelocityReductionFactor = (NetworkQualityScore < PoorConnectionThreshold) ? 0.5f : 0.3f;
// 			FVector& Velocity = MoveComp->Velocity;
// 			const float OriginalZ = Velocity.Z;
			
// 			// Smoother client-side velocity reduction for better feel
// 			FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.f);
// 			float CurrentSpeed = HorizontalVel.Size();
			
// 			if (CurrentSpeed > 0.f)
// 			{
// 				// Apply a speed-based reduction that feels more natural
// 				float SpeedReductionFactor = FMath::Clamp(CurrentSpeed / 800.f, 0.2f, 1.f);
// 				FVector ReducedDirection = HorizontalVel.GetSafeNormal();
// 				Velocity = ReducedDirection * (CurrentSpeed * VelocityReductionFactor * SpeedReductionFactor);
// 				Velocity.Z = OriginalZ;
// 			}
// 		}
// 		// Non-locally controlled clients: Don't modify velocity, let server replication handle it
		
// #if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
// 		// Only log in debug builds to avoid performance cost in shipping
// 		UE_LOG(LogTemp, Verbose, TEXT("[%s] EndDodge: Velocity transition applied"), 
// 			OwnerCharacter->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));
// #endif
// 	}
	
// 	// Clear the dodge end timer
// 	if (World)
// 	{
// 		World->GetTimerManager().ClearTimer(DodgeEndTimerHandle);
// 	}
	
// 	// Clear history and dodge velocity
// 	PositionHistory.Empty();
// 	DodgeVelocity = FVector::ZeroVector;
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Re-enable UpdateDodge for smooth client-side handling
	// UpdateDodge(DeltaTime);
	
#if WITH_EDITOR
	if (bIsDodging && OwnerCharacter.IsValid())
	{
		// Debug visualization
		DrawDebugSphere(GetWorld(), OwnerCharacter->GetActorLocation(), 40.f, 12, 
			bIsInvincible ? FColor::Yellow : FColor::Blue, false, 0.01f);
		
		// Show dodge direction
		FVector Start = OwnerCharacter->GetActorLocation();
		FVector End = Start + GetLookDirection() * 200.f;
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.01f, 0, 2.f);
	}
#endif
}

// ============== Attack System (unchanged except dodge montage dependency removed) ==============
void UCombatComponent::PlayMontage(UAnimMontage* Montage, FOnMontageEnded& Delegate)
{
	if (!OwnerCharacter.IsValid() || !Montage) return;
	if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(Montage);
		AnimInstance->Montage_SetEndDelegate(Delegate, Montage);
	}
}

void UCombatComponent::StartAttack()
{
	if (!OwnerCharacter.IsValid() || bIsDodging) return; // can't attack mid-dodge
	UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	OwnerCharacter->SwitchToWalking();
	AttackMontageDelegate.Unbind();

	if (bIsSecondAttackWindowOpen && !AnimInstance->Montage_IsPlaying(OwnerCharacter->GetSecondAttackMontage()))
	{
		bIsAttacking = true;
		AttackMontageDelegate.BindUObject(this, &UCombatComponent::FinishAttack);
		PlayMontage(OwnerCharacter->GetSecondAttackMontage(), AttackMontageDelegate);
		return;
	}
	if ((!bIsAttacking || bIsAttackEnding) && !AnimInstance->Montage_IsPlaying(OwnerCharacter->GetFirstAttackMontage()))
	{
		bIsAttacking = true;
		AttackMontageDelegate.BindUObject(this, &UCombatComponent::FinishAttack);
		PlayMontage(OwnerCharacter->GetFirstAttackMontage(), AttackMontageDelegate);
	}
}

void UCombatComponent::FinishAttack(UAnimMontage* Montage, bool bInterrupted)
{
	bIsAttacking = false;
	if (OwnerCharacter.IsValid() && !bInterrupted)
	{
		OwnerCharacter->SwitchToRunning();
	}
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, bIsDodging);
	DOREPLIFETIME(UCombatComponent, bIsInvincible);
	DOREPLIFETIME(UCombatComponent, bIsAttacking);
	DOREPLIFETIME(UCombatComponent, bIsAttackEnding);
	DOREPLIFETIME(UCombatComponent, bIsSecondAttackWindowOpen);
	DOREPLIFETIME(UCombatComponent, ReplicatedDodgeDirection);
	DOREPLIFETIME(UCombatComponent, DodgeStartTime);
}

// void UCombatComponent::OnRep_SecondAttackWindow()
// {
// 	if (OwnerCharacter.IsValid())
// 	{
// 		UE_LOG(LogTemp, Verbose, TEXT("Second attack window replicated: %s"), bIsSecondAttackWindowOpen ? TEXT("Open") : TEXT("Closed"));
// 	}
// }

#include "CompanionCharacterMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Templates/UnrealTemplate.h"

UCompanionCharacterMovementComponent::UCompanionCharacterMovementComponent()
{
	// The template locomotion animation checks acceleration as well as velocity.
#if UE_VERSION_OLDER_THAN(5, 5, 0)
	bUseAccelerationForPaths = true;
#else
	NavMovementProperties.bUseAccelerationForPaths = true;
#endif
}

static TAutoConsoleVariable<int32> CVarCompanionWallRunDebug(
	TEXT("companion.WallRun.Debug"), 0,
	TEXT("Show wall-run status and side detection lines for the controlled companion (0/1)."));

FString UCompanionCharacterMovementComponent::GetWallRunDebugStatus() const
{
	if (bWallRunning) return TEXT("RUNNING");
	if (!bCanWallRun) return TEXT("Disabled: this character has not been given wall running");
	if (!IsFalling()) return TEXT("Grounded: jump while moving alongside the wall");
	if (bWallRunUsedThisAirborne && !bWallTransferActive) return TEXT("Run ended: land to reset, or jump off while running to transfer");
	if (Acceleration.IsNearlyZero()) return TEXT("No movement input");
	if (Velocity.SizeSquared2D() < FMath::Square(MinWallRunSpeed)) return TEXT("Below minimum horizontal speed");

	const FVector TravelDirection = GetWallSearchDirection();
	const FVector SideDirection = FVector::CrossProduct(FVector::UpVector, TravelDirection);
	bool bFoundWall = false;
	for (float Side : {1.0f, -1.0f})
	{
		FHitResult Hit;
		if (!FindWall(SideDirection * Side, Hit)) continue;
		if (!CanCatchWall(Hit.ImpactNormal.GetSafeNormal2D())) continue;
		bFoundWall = true;
		FVector AlongWall = FVector::CrossProduct(Hit.ImpactNormal.GetSafeNormal2D(), FVector::UpVector);
		if (FVector::DotProduct(AlongWall, TravelDirection) < 0.0f) AlongWall *= -1.0f;
		if (FVector::DotProduct(AlongWall, TravelDirection) >= 0.7f
			&& FVector::DotProduct(Acceleration.GetSafeNormal2D(), AlongWall) > 0.1f)
		{
			return TEXT("Ready: valid side wall detected");
		}
	}
	if (bWallTransferActive && !bFoundWall) return TEXT("Transferring: waiting for an opposing wall");
	return bFoundWall ? TEXT("Wall detected: move along it, not into it")
		: TEXT("No stationary side wall: check distance, height and Pawn-blocking collision");
}

void UCompanionCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CVarCompanionWallRunDebug.GetValueOnGameThread() || !HasValidData() || !HasPlayerControl())
	{
		LastWallRunDebugStatus.Reset();
		return;
	}
	const FString Status = GetWallRunDebugStatus();
	if (Status != LastWallRunDebugStatus)
	{
		UE_LOG(LogTemp, Display, TEXT("WallRun [%s]: %s"), *CharacterOwner->GetName(), *Status);
		LastWallRunDebugStatus = Status;
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.1f, bWallRunning ? FColor::Green : FColor::Yellow,
			FString::Printf(TEXT("WallRun [%s]: %s | Speed %.0f | Reach %.0f cm"),
				*CharacterOwner->GetName(), *Status, Velocity.Size2D(), WallDetectionDistance));
	}
	const FVector TravelDirection = Velocity.IsNearlyZero() ? CharacterOwner->GetActorForwardVector() : GetWallSearchDirection();
	const FVector SideDirection = FVector::CrossProduct(FVector::UpVector, TravelDirection);
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const float Reach = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + WallDetectionDistance;
	for (float Side : {1.0f, -1.0f})
	{
		FHitResult Hit;
		const bool bHit = FindWall(SideDirection * Side, Hit);
		DrawDebugLine(GetWorld(), Start, Start + SideDirection * Side * Reach,
			bHit ? FColor::Green : FColor::Red, false, -1.0f, 0, 2.0f);
	}
}

void UCompanionCharacterMovementComponent::SetWallRunEnabled(bool bEnabled)
{
	bCanWallRun = bEnabled;
	if (!bEnabled)
	{
		StopWallRun();
	}
}

void UCompanionCharacterMovementComponent::StopWallRun()
{
	bWallRunning = false;
	bWallTransferActive = false;
	WallTransferTimeRemaining = 0.0f;
}

bool UCompanionCharacterMovementComponent::IsWallOnRight() const
{
	const FVector TravelRight = FVector::CrossProduct(FVector::UpVector, WallRunDirection);
	return bWallRunning && FVector::DotProduct(TravelRight, -WallNormal) > 0.0f;
}

bool UCompanionCharacterMovementComponent::JumpOffWall()
{
	if (!bWallRunning || !HasPlayerControl())
	{
		return false;
	}

	StopWallRun();
	// Keep the last wall normal and forward direction until the next catch. Opposing
	// surfaces can belong to the same actor/component, so actor identity is not used.
	bWallTransferActive = WallTransferDuration > 0.0f;
	WallTransferTimeRemaining = WallTransferDuration;
	Velocity = WallRunDirection * WallRunSpeed + WallNormal * WallJumpAwaySpeed;
	Velocity.Z = WallJumpUpSpeed;
	return true;
}

float UCompanionCharacterMovementComponent::GetMaxSpeed() const
{
	if (bWallRunning) return WallRunSpeed;
	// Walking's speed cap must not erase the combined forward/sideways launch.
	return bWallTransferActive ? FMath::Max(WallRunSpeed, static_cast<float>(Velocity.Size2D())) : Super::GetMaxSpeed();
}

void UCompanionCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (!IsFalling())
	{
		StopWallRun();
	}
	if (IsMovingOnGround())
	{
		bWallRunUsedThisAirborne = false;
	}
}

bool UCompanionCharacterMovementComponent::FindWall(const FVector& Direction, FHitResult& Hit) const
{
	const float Reach = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + WallDetectionDistance;
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(CompanionWallRun), false, CharacterOwner);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	return GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Direction * Reach, Objects, Query)
		&& Hit.bBlockingHit
		&& Hit.GetComponent()
		&& Hit.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
		// Blueprint blockout walls often use WorldDynamic despite being stationary.
		// Support those without changing their collision profiles or mobility.
		&& !Hit.GetComponent()->IsSimulatingPhysics()
		&& Hit.GetComponent()->GetComponentVelocity().IsNearlyZero(1.0f)
		&& FMath::Abs(Hit.ImpactNormal.Z) <= 0.2f;
}

bool UCompanionCharacterMovementComponent::HasWallRunInput() const
{
	return FVector::DotProduct(Acceleration.GetSafeNormal2D(), WallRunDirection) > 0.1f;
}

bool UCompanionCharacterMovementComponent::HasPlayerControl() const
{
	return CharacterOwner && Cast<APlayerController>(CharacterOwner->GetController()) != nullptr;
}

FVector UCompanionCharacterMovementComponent::GetWallSearchDirection() const
{
	// The launch is diagonal across the corridor. Search beside the original
	// forward direction so even a strong sideways push can catch the next wall.
	return bWallTransferActive ? WallRunDirection : Velocity.GetSafeNormal2D();
}

bool UCompanionCharacterMovementComponent::CanCatchWall(const FVector& Normal) const
{
	// Only a deliberate jump can refresh a used run, and only on an opposing face.
	// The wall just left remains ineligible throughout the transfer.
	return !bWallRunUsedThisAirborne
		|| (bWallTransferActive && FVector::DotProduct(Normal, WallNormal) <= -0.7f);
}

void UCompanionCharacterMovementComponent::TryStartWallRun()
{
	if (!bCanWallRun || (bWallRunUsedThisAirborne && !bWallTransferActive) || !HasPlayerControl()
		|| Acceleration.IsNearlyZero() || Velocity.SizeSquared2D() < FMath::Square(MinWallRunSpeed))
	{
		return;
	}

	const FVector TravelDirection = GetWallSearchDirection();
	const FVector SideDirection = FVector::CrossProduct(FVector::UpVector, TravelDirection);
	for (float Side : {1.0f, -1.0f})
	{
		FHitResult Hit;
		if (!FindWall(SideDirection * Side, Hit))
		{
			continue;
		}

		const FVector CandidateNormal = Hit.ImpactNormal.GetSafeNormal2D();
		if (!CanCatchWall(CandidateNormal)) continue;
		FVector CandidateDirection = FVector::CrossProduct(CandidateNormal, FVector::UpVector);
		if (FVector::DotProduct(CandidateDirection, TravelDirection) < 0.0f)
		{
			CandidateDirection *= -1.0f;
		}
		if (FVector::DotProduct(CandidateDirection, TravelDirection) < 0.7f
			|| FVector::DotProduct(Acceleration.GetSafeNormal2D(), CandidateDirection) <= 0.1f)
		{
			continue;
		}

		WallNormal = CandidateNormal;
		WallRunDirection = CandidateDirection;
		bWallTransferActive = false;
		WallTransferTimeRemaining = 0.0f;
		bWallRunning = true;
		bWallRunUsedThisAirborne = true;
		WallRunElapsed = 0.0f;
		Velocity.Z = FMath::Clamp(Velocity.Z, -100.0, 100.0);
		return;
	}
}

void UCompanionCharacterMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
	if (!HasValidData())
	{
		return;
	}
	if (bWallTransferActive)
	{
		WallTransferTimeRemaining -= DeltaTime;
		if (!bCanWallRun || !HasPlayerControl() || !HasWallRunInput() || WallTransferTimeRemaining <= 0.0f)
		{
			StopWallRun();
		}
	}

	if (bWallRunning)
	{
		FHitResult Hit;
		if (!bCanWallRun || !HasPlayerControl() || !HasWallRunInput()
			|| WallRunElapsed >= MaxWallRunDuration
			|| !FindWall(-WallNormal, Hit) || FVector::DotProduct(Hit.ImpactNormal, WallNormal) < 0.95f)
		{
			StopWallRun();
		}
	}
	else
	{
		TryStartWallRun();
	}

	if (!bWallRunning)
	{
		if (bWallTransferActive)
		{
			// Preserve the jump's horizontal trajectory while native physics handles
			// full gravity, capsule collisions and landing. No launch is added per tick.
			TGuardValue<float> FrictionGuard(FallingLateralFriction, 0.0f);
			TGuardValue<float> BrakingGuard(BrakingDecelerationFalling, 0.0f);
			TGuardValue<FVector> AccelerationGuard(Acceleration, FVector::ZeroVector);
			Super::PhysFalling(DeltaTime, Iterations);
		}
		else
		{
			Super::PhysFalling(DeltaTime, Iterations);
		}
		return;
	}

	// Native falling physics retains capsule sweeps, sliding, and landing handling.
	// Limit the run portion of this frame so its duration does not depend on frame rate.
	const float RunTime = FMath::Min(DeltaTime, FMath::Max(MaxWallRunDuration - WallRunElapsed, 0.0f));
	WallRunElapsed += RunTime;
	Velocity.X = WallRunDirection.X * WallRunSpeed;
	Velocity.Y = WallRunDirection.Y * WallRunSpeed;
	{
		TGuardValue<float> GravityGuard(GravityScale, GravityScale * WallRunGravityMultiplier);
		TGuardValue<float> FrictionGuard(FallingLateralFriction, 0.0f);
		TGuardValue<float> BrakingGuard(BrakingDecelerationFalling, 0.0f);
		TGuardValue<FVector> AccelerationGuard(Acceleration, FVector::ZeroVector);
		Super::PhysFalling(RunTime, Iterations);
	}

	if (WallRunElapsed >= MaxWallRunDuration || Velocity.SizeSquared2D() < FMath::Square(MinWallRunSpeed))
	{
		StopWallRun();
	}
	if (DeltaTime > RunTime)
	{
		StartNewPhysics(DeltaTime - RunTime, Iterations);
	}
}

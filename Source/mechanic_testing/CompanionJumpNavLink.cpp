#include "CompanionJumpNavLink.h"

#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersionComparison.h"
#include "NavLinkCustomComponent.h"
#include "Navigation/PathFollowingComponent.h"

ACompanionJumpNavLink::ACompanionJumpNavLink(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bSmartLinkIsRelevant = true;
	PointLinks.Reset();
	GetSmartLinkComp()->SetLinkData(StartPoint, EndPoint, Direction);
}

void ACompanionJumpNavLink::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// This actor owns one smart link only; simple links would bypass the jump callback.
	PointLinks.Reset();
	GetSmartLinkComp()->SetLinkData(StartPoint, EndPoint, Direction);
}

void ACompanionJumpNavLink::BeginPlay()
{
	Super::BeginPlay();
	OnSmartLinkReached.AddUniqueDynamic(this, &ACompanionJumpNavLink::HandleSmartLinkReached);
}

bool ACompanionJumpNavLink::BuildJumpPlan(ACharacter* CompanionPawn, const FVector& Destination,
	FVector& Velocity, FVector& LandingPoint, FString& FailureReason, float* SecondJumpTime) const
{
	if (SecondJumpTime) *SecondJumpTime = -1.0f;
	if (!IsValid(CompanionPawn) || !CompanionPawn->GetCharacterMovement()->IsMovingOnGround())
	{
		FailureReason = TEXT("the character must start on the ground");
		return false;
	}
	UCharacterMovementComponent* Movement = CompanionPawn->GetCharacterMovement();
	UCapsuleComponent* Capsule = CompanionPawn->GetCapsuleComponent();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CompanionJumpLink), false, CompanionPawn);
	Query.AddIgnoredActor(this);
	FHitResult Floor;
	if (!GetWorld()->LineTraceSingleByChannel(Floor, Destination + FVector::UpVector * 50.0,
		Destination - FVector::UpVector * 150.0, ECC_Pawn, Query)
		|| Floor.bStartPenetrating || !Movement->IsWalkable(Floor))
	{
		FailureReason = TEXT("the destination needs clear, walkable landing ground");
		return false;
	}

	LandingPoint = Floor.ImpactPoint;
	const FVector Start = CompanionPawn->GetActorLocation();
	const FVector End = LandingPoint + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
	const FVector Delta = End - Start;
	// Downward links walk off the ledge; they must not receive a jump impulse.
	if (LandingPoint.Z < Movement->GetActorFeetLocation().Z - 5.0f)
	{
		Velocity = FVector::ZeroVector;
		return true;
	}
	const float Gravity = -Movement->GetGravityZ();
	const float JumpSpeed = Movement->JumpZVelocity;
	if (Gravity <= KINDA_SMALL_NUMBER || JumpSpeed <= 0.0f || !CompanionPawn->CanJump())
	{
		FailureReason = TEXT("the character cannot jump with its current movement settings");
		return false;
	}

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	const auto CheckArc = [&](const FVector& ArcStart, const FVector& ArcVelocity, float Duration)
	{
		FPredictProjectilePathParams Parameters;
		Parameters.StartLocation = ArcStart;
		Parameters.LaunchVelocity = ArcVelocity;
		Parameters.OverrideGravityZ = Movement->GetGravityZ();
		Parameters.MaxSimTime = Duration;
		Parameters.SimFrequency = 60.0f;
		Parameters.bTraceWithCollision = false;
		FPredictProjectilePathResult Prediction;
		UGameplayStatics::PredictProjectilePath(this, Parameters, Prediction);
		if (Prediction.PathData.Num() < 2)
		{
			FailureReason = TEXT("could not predict a jump trajectory");
			return false;
		}
		for (int32 Index = 1; Index < Prediction.PathData.Num(); ++Index)
		{
			const FVector From = Prediction.PathData[Index - 1].Location;
			const FVector To = Prediction.PathData[Index].Location;
			FHitResult Hit;
			const bool bBlocked = GetWorld()->SweepSingleByProfile(
				Hit, From, To, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query);
			if (bDrawJumpDebug) DrawDebugLine(GetWorld(), From, To, bBlocked ? FColor::Red : FColor::Green, false, 3.0f);
			if (bBlocked)
			{
				FailureReason = FString::Printf(TEXT("the jump arc is blocked by %s"), *GetNameSafe(Hit.GetActor()));
				return false;
			}
		}
		return true;
	};

	// Prefer one normal jump. If necessary, search bounded second-jump timings
	// during ascent, up to the apex. Both impulses use the normal JumpZVelocity.
	const auto TryPlan = [&](float BoostTime)
	{
		const float FirstHeight = BoostTime > 0.0f ? JumpSpeed * BoostTime - 0.5f * Gravity * BoostTime * BoostTime : 0.0f;
		const float Discriminant = JumpSpeed * JumpSpeed - 2.0f * Gravity * (Delta.Z - FirstHeight);
		if (Discriminant < 0.0f)
		{
			FailureReason = TEXT("the landing is above this character's available jump height");
			return false;
		}
		const float LastFlightTime = (JumpSpeed + FMath::Sqrt(Discriminant)) / Gravity;
		const float FlightTime = FMath::Max(BoostTime, 0.0f) + LastFlightTime;
		if (LastFlightTime <= KINDA_SMALL_NUMBER || FlightTime + 0.25f >= FMath::Max(TraversalTimeout, 1.0f))
		{
			FailureReason = TEXT("the jump exceeds the traversal timeout");
			return false;
		}
		const FVector Launch(Delta.X / FlightTime, Delta.Y / FlightTime, JumpSpeed);
		if (Launch.Size2D() > Movement->MaxWalkSpeed)
		{
			FailureReason = TEXT("the landing is too far for this character's normal movement speed");
			return false;
		}
		if (BoostTime > 0.0f)
		{
			if (!CheckArc(Start, Launch, BoostTime)) return false;
			FVector BoostLocation = Start + Launch * BoostTime;
			BoostLocation.Z = Start.Z + FirstHeight;
			if (!CheckArc(BoostLocation, Launch, LastFlightTime)) return false;
		}
		else if (!CheckArc(Start, Launch, FlightTime)) return false;
		Velocity = Launch;
		if (SecondJumpTime) *SecondJumpTime = BoostTime;
		FailureReason.Reset();
		return true;
	};
	if (TryPlan(-1.0f)) return true;
	if (CompanionPawn->JumpMaxCount >= 2)
	{
		for (int32 Sample = 2; Sample <= 8; ++Sample)
		{
			if (TryPlan((JumpSpeed / Gravity) * (static_cast<float>(Sample) / 8.0f))) return true;
		}
	}
	return false;
}

void ACompanionJumpNavLink::HandleSmartLinkReached(AActor* Agent, const FVector& Destination)
{
	ACharacter* CompanionPawn = Cast<ACharacter>(Agent);
	if (!CompanionPawn)
	{
		return;
	}
	AAIController* AI = Cast<AAIController>(CompanionPawn->GetController());
	UPathFollowingComponent* Path = AI ? AI->GetPathFollowingComponent() : nullptr;
	if (!Path || Path->GetStatus() == EPathFollowingStatus::Idle)
	{
		return;
	}
	// ResumeMove can select the just-completed custom segment again. Let normal
	// path following advance past it instead of launching from the landing point.
	UCharacterMovementComponent* Movement = CompanionPawn->GetCharacterMovement();
	if (Movement->IsMovingOnGround()
		&& FVector::Dist(Movement->GetActorFeetLocation(), Destination) <= FMath::Max(LandingTolerance, 10.0f))
	{
		ResumePathFollowing(CompanionPawn);
		return;
	}
	if (Traversals.Contains(CompanionPawn)) return;
	FVector Velocity;
	FVector LandingPoint;
	FString FailureReason;
	float SecondJumpTime = -1.0f;
	if (!BuildJumpPlan(CompanionPawn, Destination, Velocity, LandingPoint, FailureReason, &SecondJumpTime))
	{
		WarnRejected(FailureReason);
		AI->StopMovement();
		return;
	}

	FTraversal State;
	State.Controller = AI;
	State.RequestId = Path->GetCurrentRequestId();
	State.LandingPoint = LandingPoint;
	State.bDrop = Velocity.IsNearlyZero();
	State.SecondJumpTime = SecondJumpTime;
	State.AirControl = Movement->AirControl;
	State.FallingFriction = Movement->FallingLateralFriction;
	State.FallingBraking = Movement->BrakingDecelerationFalling;
	Path->PauseMove(State.RequestId, EPathFollowingVelocityMode::Keep);
	Traversals.Add(CompanionPawn, State);
	Movement->AirControl = 0.0f;
	Movement->FallingLateralFriction = 0.0f;
	Movement->BrakingDecelerationFalling = 0.0f;
	CompanionPawn->StopJumping();
	if (!State.bDrop)
	{
		CompanionPawn->LaunchCharacter(Velocity, true, true);
		// LaunchCharacter bypasses CheckJumpInput: account for this first jump.
		CompanionPawn->JumpCurrentCount = 1;
	}
	SetActorTickEnabled(true);
}

void ACompanionJumpNavLink::FinishTraversal(TWeakObjectPtr<ACharacter> CompanionPawn, bool bSuccess)
{
	FTraversal State;
	if (!Traversals.RemoveAndCopyValue(CompanionPawn, State))
	{
		return;
	}
	if (ACharacter* Pawn = CompanionPawn.Get())
	{
		UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
		Movement->AirControl = State.AirControl;
		Movement->FallingLateralFriction = State.FallingFriction;
		Movement->BrakingDecelerationFalling = State.FallingBraking;
		if (AAIController* AI = State.Controller.Get(); AI && Pawn->GetController() == AI)
		{
			UPathFollowingComponent* Path = AI->GetPathFollowingComponent();
			if (Path && Path->GetCurrentRequestId() == State.RequestId)
			{
				if (bSuccess)
				{
					ResumePathFollowing(Pawn);
					Path->ResumeMove(State.RequestId);
				}
				else
				{
					AI->StopMovement();
				}
			}
		}
	}
	SetActorTickEnabled(!Traversals.IsEmpty());
}

void ACompanionJumpNavLink::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TArray<TWeakObjectPtr<ACharacter>> Finished;
	TArray<TWeakObjectPtr<ACharacter>> Failed;
	for (auto& Entry : Traversals)
	{
		ACharacter* Pawn = Entry.Key.Get();
		FTraversal& State = Entry.Value;
		AAIController* AI = State.Controller.Get();
		UPathFollowingComponent* Path = AI ? AI->GetPathFollowingComponent() : nullptr;
		if (!Pawn || !AI || Pawn->GetController() != AI || !Path
			|| Path->GetCurrentRequestId() != State.RequestId || Path->GetStatus() == EPathFollowingStatus::Idle)
		{
			Failed.Add(Entry.Key);
			continue;
		}
		State.Elapsed += DeltaSeconds;
		UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
		if (State.Elapsed >= FMath::Max(TraversalTimeout, 1.0f))
		{
			WarnRejected(TEXT("the traversal timed out"));
			Failed.Add(Entry.Key);
			continue;
		}
		if (State.SecondJumpTime >= 0.0f && !State.bSecondJumpUsed
			&& Movement->IsFalling() && State.Elapsed >= State.SecondJumpTime)
		{
			// Native DoJump checks the character's remaining jump allowance and sets
			// upward speed; it does not add another impulse every frame.
			bool bJumped = false;
			if (Pawn->JumpMaxCount >= 2)
			{
#if UE_VERSION_OLDER_THAN(5, 5, 0)
				bJumped = Movement->DoJump(false);
#else
				bJumped = Movement->DoJump(false, DeltaSeconds);
#endif
			}
			if (!bJumped)
			{
				WarnRejected(TEXT("the planned second jump is no longer available"));
				Failed.Add(Entry.Key);
				continue;
			}
			++Pawn->JumpCurrentCount;
			State.bSecondJumpUsed = true;
		}
		if (State.bDrop && !State.bWasFalling)
		{
			const FVector Delta = State.LandingPoint - Movement->GetActorFeetLocation();
			if (Movement->IsMovingOnGround())
			{
				Movement->RequestPathMove(Delta.GetSafeNormal2D());
			}
			else if (Movement->IsFalling())
			{
				// Once clear of the edge, steer horizontally without an upward launch.
				const float Gravity = -Movement->GetGravityZ();
				const float Height = FMath::Max(-Delta.Z, 0.0);
				const float Time = Gravity > KINDA_SMALL_NUMBER
					? (Movement->Velocity.Z + FMath::Sqrt(FMath::Square(Movement->Velocity.Z) + 2.0f * Gravity * Height)) / Gravity : 0.0f;
				const FVector Horizontal = (Delta.GetSafeNormal2D() * (Time > KINDA_SMALL_NUMBER ? Delta.Size2D() / Time : 0.0)).GetClampedToMaxSize(Movement->MaxWalkSpeed);
				Movement->Velocity.X = Horizontal.X;
				Movement->Velocity.Y = Horizontal.Y;
			}
		}
		State.bWasFalling |= Movement->IsFalling();
		const bool bAtLanding = FVector::Dist(Movement->GetActorFeetLocation(), State.LandingPoint) <= FMath::Max(LandingTolerance, 10.0f);
		if (Movement->IsMovingOnGround() && (State.bWasFalling || (State.bDrop && bAtLanding)))
		{
			if (bAtLanding)
			{
				Finished.Add(Entry.Key);
			}
			else
			{
				WarnRejected(TEXT("the character landed outside the landing tolerance"));
				Failed.Add(Entry.Key);
			}
		}
	}
	for (const auto& Pawn : Finished) FinishTraversal(Pawn, true);
	for (const auto& Pawn : Failed) FinishTraversal(Pawn, false);
}

void ACompanionJumpNavLink::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnSmartLinkReached.RemoveDynamic(this, &ACompanionJumpNavLink::HandleSmartLinkReached);
	TArray<TWeakObjectPtr<ACharacter>> Active;
	Traversals.GetKeys(Active);
	for (const auto& Pawn : Active) FinishTraversal(Pawn, false);
	Super::EndPlay(EndPlayReason);
}

void ACompanionJumpNavLink::WarnRejected(const FString& Reason)
{
	if (GetWorld()->GetTimeSeconds() - LastWarningTime >= 2.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump link %s: %s."), *GetName(), *Reason);
		LastWarningTime = GetWorld()->GetTimeSeconds();
	}
}

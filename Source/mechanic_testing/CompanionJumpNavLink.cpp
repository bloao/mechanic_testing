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
	FVector& Velocity, FVector& LandingPoint, FString& FailureReason) const
{
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
	const float Gravity = -Movement->GetGravityZ();
	const float JumpSpeed = Movement->JumpZVelocity;
	const float Discriminant = JumpSpeed * JumpSpeed - 2.0f * Gravity * Delta.Z;
	if (Gravity <= KINDA_SMALL_NUMBER || JumpSpeed <= 0.0f || Discriminant < 0.0f)
	{
		FailureReason = TEXT("the landing is above this character's normal jump height");
		return false;
	}

	// Use the character's normal vertical jump speed, solving for the descending landing time.
	const float FlightTime = (JumpSpeed + FMath::Sqrt(Discriminant)) / Gravity;
	if (FlightTime <= KINDA_SMALL_NUMBER || FlightTime + 0.25f >= FMath::Max(TraversalTimeout, 1.0f))
	{
		FailureReason = TEXT("the jump exceeds the traversal timeout");
		return false;
	}
	Velocity = FVector(Delta.X / FlightTime, Delta.Y / FlightTime, JumpSpeed);
	if (Velocity.Size2D() > Movement->MaxWalkSpeed)
	{
		FailureReason = TEXT("the landing is too far for this character's normal movement speed");
		return false;
	}

	// Let Unreal sample the ballistic arc, then sweep the full character capsule along it.
	FPredictProjectilePathParams Parameters;
	Parameters.StartLocation = Start;
	Parameters.LaunchVelocity = Velocity;
	Parameters.OverrideGravityZ = Movement->GetGravityZ();
	Parameters.MaxSimTime = FlightTime;
	Parameters.SimFrequency = 30.0f;
	Parameters.bTraceWithCollision = false;
	FPredictProjectilePathResult Prediction;
	UGameplayStatics::PredictProjectilePath(this, Parameters, Prediction);
	if (Prediction.PathData.Num() < 2)
	{
		FailureReason = TEXT("could not predict a jump trajectory");
		return false;
	}
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	for (int32 Index = 1; Index < Prediction.PathData.Num(); ++Index)
	{
		const FVector From = Prediction.PathData[Index - 1].Location;
		const FVector To = Prediction.PathData[Index].Location;
		FHitResult Hit;
		const bool bBlocked = GetWorld()->SweepSingleByProfile(
			Hit, From, To, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query);
		if (bDrawJumpDebug)
		{
			DrawDebugLine(GetWorld(), From, To, bBlocked ? FColor::Red : FColor::Green, false, 3.0f);
		}
		if (bBlocked)
		{
			FailureReason = FString::Printf(TEXT("the jump arc is blocked by %s"), *GetNameSafe(Hit.GetActor()));
			return false;
		}
	}
	return true;
}

void ACompanionJumpNavLink::HandleSmartLinkReached(AActor* Agent, const FVector& Destination)
{
	ACharacter* CompanionPawn = Cast<ACharacter>(Agent);
	if (!CompanionPawn || Traversals.Contains(CompanionPawn))
	{
		return;
	}
	AAIController* AI = Cast<AAIController>(CompanionPawn->GetController());
	UPathFollowingComponent* Path = AI ? AI->GetPathFollowingComponent() : nullptr;
	if (!Path || Path->GetStatus() == EPathFollowingStatus::Idle)
	{
		return;
	}
	FVector Velocity;
	FVector LandingPoint;
	FString FailureReason;
	if (!BuildJumpPlan(CompanionPawn, Destination, Velocity, LandingPoint, FailureReason))
	{
		WarnRejected(FailureReason);
		AI->StopMovement();
		return;
	}

	UCharacterMovementComponent* Movement = CompanionPawn->GetCharacterMovement();
	FTraversal State;
	State.Controller = AI;
	State.RequestId = Path->GetCurrentRequestId();
	State.LandingPoint = LandingPoint;
	State.AirControl = Movement->AirControl;
	State.FallingFriction = Movement->FallingLateralFriction;
	State.FallingBraking = Movement->BrakingDecelerationFalling;
	Path->PauseMove(State.RequestId, EPathFollowingVelocityMode::Keep);
	Traversals.Add(CompanionPawn, State);
	Movement->AirControl = 0.0f;
	Movement->FallingLateralFriction = 0.0f;
	Movement->BrakingDecelerationFalling = 0.0f;
	CompanionPawn->StopJumping();
	CompanionPawn->LaunchCharacter(Velocity, true, true);
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
		State.bWasFalling |= Movement->IsFalling();
		if (State.bWasFalling && Movement->IsMovingOnGround())
		{
			if (FVector::Dist(Movement->GetActorFeetLocation(), State.LandingPoint) <= FMath::Max(LandingTolerance, 10.0f))
			{
				Finished.Add(Entry.Key);
			}
			else
			{
				WarnRejected(TEXT("the character landed outside the landing tolerance"));
				Failed.Add(Entry.Key);
			}
		}
		else if (State.Elapsed >= FMath::Max(TraversalTimeout, 1.0f))
		{
			WarnRejected(TEXT("the traversal timed out"));
			Failed.Add(Entry.Key);
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


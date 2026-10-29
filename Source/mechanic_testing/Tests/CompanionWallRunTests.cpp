#if WITH_DEV_AUTOMATION_TESTS

#include "mechanic_testing/CompanionCharacterBase.h"
#include "mechanic_testing/CompanionCharacterMovementComponent.h"
#include "mechanic_testing/CompanionGameMode.h"
#include "mechanic_testing/CompanionJumpNavLink.h"
#include "mechanic_testing/CompanionSwitchPlayerController.h"
#include "mechanic_testing/BreakableActor.h"
#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavLinkCustomComponent.h"

namespace CompanionWallRunTests
{
	struct FTestWorld
	{
		UWorld* World;
		APlayerController* Player;
		ACompanionCharacterBase* Character;
		UCompanionCharacterMovementComponent* Movement;
		bool bFirstStep = true;

		explicit FTestWorld(float WallSide = 1.0f)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			AddBox(FVector(0.0, 0.0, -50.0), FVector(5000.0, 5000.0, 50.0));
			if (WallSide != 0.0f)
			{
				AddBox(FVector(0.0, WallSide * 75.0, 1000.0), FVector(5000.0, 10.0, 1000.0));
			}
			Character = World->SpawnActor<ACompanionCharacterBase>(FVector(0.0, 0.0, 500.0), FRotator::ZeroRotator);
			Player = World->SpawnActor<APlayerController>();
			Player->Possess(Character);
			Movement = CastChecked<UCompanionCharacterMovementComponent>(Character->GetCharacterMovement());
			Character->ConfigureAgileAbilities(true);
			Movement->SetMovementMode(MOVE_Falling);
			Movement->Velocity = FVector(600.0, 0.0, 0.0);
		}

		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void AddBox(const FVector& Location, const FVector& Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->InitBoxExtent(Extent);
			Box->SetWorldLocation(Location);
			Box->SetMobility(EComponentMobility::Static);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
		}

		void Step(bool bMove = true)
		{
			if (bMove)
			{
				Character->AddMovementInput(FVector::ForwardVector, 1.0f);
			}
			const FVector Input = Character->GetPendingMovementInputVector();
			// Advance movement directly; repeated world ticks in one engine frame are deduplicated.
			Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, &Movement->PrimaryComponentTick);
			if (bFirstStep && bMove && Movement->bCanWallRun && !Movement->IsWallRunning() && Cast<APlayerController>(Character->GetController()))
			{
				FHitResult RightHit;
				FHitResult LeftHit;
				const FVector Start = Character->GetActorLocation();
				const float Reach = Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + Movement->WallDetectionDistance;
				const FCollisionQueryParams Query(SCENE_QUERY_STAT(WallRunTest), false, Character);
				const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
				const bool bRight = World->LineTraceSingleByObjectType(RightHit, Start, Start + FVector::RightVector * Reach, Objects, Query);
				const bool bLeft = World->LineTraceSingleByObjectType(LeftHit, Start, Start - FVector::RightVector * Reach, Objects, Query);
				UE_LOG(LogTemp, Display, TEXT("WallRun fixture: local=%d mode=%d input=%s accel=%s velocity=%s position=%s right=%d left=%d"),
					Character->IsLocallyControlled(), static_cast<int32>(Movement->MovementMode), *Input.ToString(),
					*Movement->GetCurrentAcceleration().ToString(), *Movement->Velocity.ToString(), *Start.ToString(), bRight, bLeft);
			}
			bFirstStep = false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionWallRunEligibilityTest,
	"MechanicTesting.WallRun.Eligibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionWallRunEligibilityTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	{
		FTestWorld TestWorld;
		const float NearGroundHeight = TestWorld.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.0f;
		TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, NearGroundHeight));
		TestWorld.Step();
		TestTrue(TEXT("Wall run can start only ten centimeters above the floor"), TestWorld.Movement->IsWallRunning());
	}
	for (float Side : {1.0f, -1.0f})
	{
		FTestWorld TestWorld(Side);
		TestWorld.Step();
		TestTrue(TEXT("Agile character can run on either side wall"), TestWorld.Movement->IsWallRunning());
		TestEqual(TEXT("Agile keeps two jumps"), TestWorld.Character->JumpMaxCount, 2);
		TestEqual(TEXT("Gravity setting is restored after wall-run simulation"), TestWorld.Movement->GravityScale, 1.0f);
		TestWorld.Step(false);
		TestFalse(TEXT("Releasing movement ends the wall run"), TestWorld.Movement->IsWallRunning());
		TestWorld.Step();
		TestFalse(TEXT("Cannot restart in the same airborne period"), TestWorld.Movement->IsWallRunning());
	}
	{
		FTestWorld TestWorld;
		TestWorld.Character->ConfigureAgileAbilities(false);
		TestWorld.Step();
		TestFalse(TEXT("Strong character cannot wall run"), TestWorld.Movement->IsWallRunning());
		TestEqual(TEXT("Strong character keeps a normal jump"), TestWorld.Character->JumpMaxCount, 1);
	}
	{
		FTestWorld TestWorld;
		TestWorld.World->SpawnActor<AAIController>()->Possess(TestWorld.Character);
		TestWorld.Character->AddMovementInput(FVector::ForwardVector, 1.0f, true);
		TestWorld.Step();
		TestFalse(TEXT("AI companion cannot start wall running"), TestWorld.Movement->IsWallRunning());
	}
	{
		FTestWorld TestWorld(0.0f);
		TestWorld.Step();
		TestFalse(TEXT("No wall means no wall run"), TestWorld.Movement->IsWallRunning());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionWallRunAnimationStateTest,
	"MechanicTesting.WallRun.AnimationState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionWallRunAnimationStateTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	for (float Side : {1.0f, -1.0f})
	{
		for (float Travel : {1.0f, -1.0f})
		{
			FTestWorld TestWorld(Side);
			TestFalse(TEXT("Idle movement does not expose a stale wall side"), TestWorld.Movement->IsWallOnRight());
			// Camera/actor facing must not reverse the animation's travel-relative side.
			TestWorld.Character->SetActorRotation(FRotator(0.0, 90.0, 0.0));
			TestWorld.Movement->Velocity = FVector::ForwardVector * (600.0f * Travel);
			TestWorld.Character->AddMovementInput(FVector::ForwardVector, Travel);
			TestWorld.Step(false);
			TestTrue(TEXT("Animation state reports an active wall run"), TestWorld.Movement->IsWallRunning());
			TestEqual(TEXT("Animation side follows the wall and direction of travel"), TestWorld.Movement->IsWallOnRight(), Side * Travel > 0.0f);
			TestWorld.Movement->SetWallRunEnabled(false);
			TestFalse(TEXT("Disabling the ability clears the animation state"), TestWorld.Movement->IsWallRunning());
			TestFalse(TEXT("Disabling the ability clears the animation side"), TestWorld.Movement->IsWallOnRight());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionWallRunExitTest,
	"MechanicTesting.WallRun.JumpAndPossession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionWallRunExitTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	{
		FTestWorld TestWorld;
		TestWorld.Character->Jump();
		TestWorld.Step();
		TestTrue(TEXT("Entered wall run before jump-off"), TestWorld.Movement->IsWallRunning());
		TestWorld.Character->Jump();
		TestTrue(TEXT("Holding the initial jump does not immediately jump off"), TestWorld.Movement->IsWallRunning());
		TestWorld.Character->StopJumping();
		TestWorld.Character->Jump();
		TestFalse(TEXT("Jump exits wall running"), TestWorld.Movement->IsWallRunning());
		TestTrue(TEXT("Jump pushes away from right-hand wall"), TestWorld.Movement->Velocity.Y < 0.0);
		TestTrue(TEXT("Jump has upward velocity"), TestWorld.Movement->Velocity.Z > 0.0);
		const FVector JumpVelocity = TestWorld.Movement->Velocity;
		TestWorld.Character->Jump();
		TestTrue(TEXT("Held jump does not retrigger"), TestWorld.Movement->Velocity.Equals(JumpVelocity));
	}
	{
		FTestWorld TestWorld;
		TestWorld.Step();
		TestTrue(TEXT("Entered wall run before possession change"), TestWorld.Movement->IsWallRunning());
		TestWorld.Player->UnPossess();
		TestFalse(TEXT("Switching away immediately ends wall run"), TestWorld.Movement->IsWallRunning());
		TestEqual(TEXT("Switching retains normal gravity"), TestWorld.Movement->GravityScale, 1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionWallRunDurationTest,
	"MechanicTesting.WallRun.DurationAndLanding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionWallRunDurationTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	FTestWorld TestWorld;
	TestWorld.Movement->MaxWallRunDuration = 0.1f;
	TestWorld.Step();
	TestTrue(TEXT("Entered limited wall run"), TestWorld.Movement->IsWallRunning());
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		TestWorld.Step();
	}
	TestFalse(TEXT("Wall run expires while still alongside the wall"), TestWorld.Movement->IsWallRunning());
	TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, 150.0));
	TestWorld.Movement->Velocity = FVector(0.0, 0.0, -200.0);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		TestWorld.Step(false);
	}
	TestTrue(TEXT("Native falling physics lands normally"), TestWorld.Movement->IsMovingOnGround());
	TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, 500.0));
	TestWorld.Movement->SetMovementMode(MOVE_Falling);
	TestWorld.Movement->Velocity = FVector(600.0, 0.0, 0.0);
	TestWorld.Step();
	TestTrue(TEXT("Landing allows a new wall run"), TestWorld.Movement->IsWallRunning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionPairSetupTest,
	"MechanicTesting.Companions.PairAndSwitching", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionPairSetupTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	FTestWorld TestWorld(0.0f);
	TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, 90.0));
	TestWorld.Player->UnPossess();
	AddExpectedMessage(TEXT("Companion setup needs two characters."), ELogVerbosity::Warning);
	ACompanionSwitchPlayerController* CompanionController = TestWorld.World->SpawnActor<ACompanionSwitchPlayerController>();
	CompanionController->Possess(TestWorld.Character);
	CompanionController->InitializeCompanions();
	ACompanionGameMode* Mode = TestWorld.World->SpawnActor<ACompanionGameMode>();
	// Test spawning/roles without requiring the mannequin or animation assets.
	Mode->StrongCompanionClass = ACompanionCharacterBase::StaticClass();
	if (!TestTrue(TEXT("GameMode creates the missing companion"), Mode->SetupCompanionPair(CompanionController))) return false;
	ACompanionCharacterBase* StrongPawn = Mode->StrongCompanion;
	TestEqual(TEXT("Agile starts controlled"), CompanionController->GetControlledCompanion(), static_cast<ACharacter*>(TestWorld.Character));
	TestEqual(TEXT("Agile has two jumps"), TestWorld.Character->JumpMaxCount, 2);
	TestEqual(TEXT("Strong has one jump"), StrongPawn->JumpMaxCount, 1);
	TestTrue(TEXT("Only strong can smash"), StrongPawn->CanPerformSmash());
	TestFalse(TEXT("Agile cannot smash"), TestWorld.Character->CanPerformSmash());
	TestTrue(TEXT("Agile wall running enabled"), TestWorld.Movement->bCanWallRun);
	TestFalse(TEXT("Strong wall running disabled"), CastChecked<UCompanionCharacterMovementComponent>(StrongPawn->GetCharacterMovement())->bCanWallRun);
	for (int32 SwitchIndex = 0; SwitchIndex < 20; ++SwitchIndex)
	{
		CompanionController->SwitchControlledCharacter();
		TestEqual(TEXT("Repeated switching selects the other pawn"), CompanionController->GetControlledCompanion(),
			static_cast<ACharacter*>(SwitchIndex % 2 == 0 ? StrongPawn : TestWorld.Character));
		TestNotNull(TEXT("Inactive companion has AI"), Cast<AAIController>(CompanionController->GetFollowerCompanion()->GetController()));
		int32 AIControllerCount = 0;
		for (TActorIterator<AAIController> It(TestWorld.World); It; ++It) ++AIControllerCount;
		TestEqual(TEXT("Switching does not accumulate unused AI controllers"), AIControllerCount, 1);
	}
	TestTrue(TEXT("Setup can run repeatedly"), Mode->SetupCompanionPair(CompanionController));
	TestEqual(TEXT("Existing strong companion is reused"), Mode->StrongCompanion.Get(), StrongPawn);
	int32 CharacterCount = 0;
	for (TActorIterator<ACharacter> It(TestWorld.World); It; ++It) ++CharacterCount;
	TestEqual(TEXT("Exactly two characters, no duplicate spawns"), CharacterCount, 2);
	AddExpectedMessage(TEXT("Companion setup requires two different valid characters."), ELogVerbosity::Warning);
	CompanionController->SetCompanionPair(TestWorld.Character, TestWorld.Character);
	TestEqual(TEXT("Invalid pair does not replace the working pair"), CompanionController->GetFollowerCompanion(), static_cast<ACharacter*>(StrongPawn));

	// Keep the smash sweep clear of the floor in this smaller base-character fixture.
	StrongPawn->SetActorLocation(StrongPawn->GetActorLocation() + FVector::UpVector * 20.0);
	ABreakableActor* Breakable = TestWorld.World->SpawnActor<ABreakableActor>(StrongPawn->GetActorLocation() + FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator);
	UStaticMeshComponent* BreakableMesh = Breakable->FindComponentByClass<UStaticMeshComponent>();
	// A primitive collider avoids waiting for asynchronous static-mesh compilation.
	UBoxComponent* HitBox = NewObject<UBoxComponent>(Breakable);
	HitBox->SetupAttachment(Breakable->GetRootComponent());
	HitBox->InitBoxExtent(FVector(50.0));
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HitBox->SetCollisionObjectType(ECC_WorldDynamic);
	HitBox->SetCollisionResponseToAllChannels(ECR_Block);
	HitBox->RegisterComponent();
	TestTrue(TEXT("Test breakable has physics collision"), HitBox->IsPhysicsStateCreated());
	TestFalse(TEXT("Agile cannot trigger a smash"), TestWorld.Character->PerformSmash());
	TestFalse(TEXT("Agile leaves the breakable intact"), Breakable->IsBroken());
	TestTrue(TEXT("Strong smashes a nearby breakable"), StrongPawn->PerformSmash());
	TestTrue(TEXT("Breakable applies its broken state"), Breakable->IsBroken());
	TestFalse(TEXT("Broken mesh is hidden"), BreakableMesh->IsVisible());
	TestEqual(TEXT("Broken mesh collision is disabled"), BreakableMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionPairNoFloorTest,
	"MechanicTesting.Companions.NoFloor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionPairNoFloorTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	FTestWorld TestWorld(0.0f);
	TestWorld.Character->SetActorLocation(FVector(10000.0, 0.0, 90.0));
	TestWorld.Player->UnPossess();
	AddExpectedMessage(TEXT("Companion setup needs two characters."), ELogVerbosity::Warning);
	ACompanionSwitchPlayerController* CompanionController = TestWorld.World->SpawnActor<ACompanionSwitchPlayerController>();
	CompanionController->Possess(TestWorld.Character);
	CompanionController->InitializeCompanions();
	ACompanionGameMode* Mode = TestWorld.World->SpawnActor<ACompanionGameMode>();
	Mode->StrongCompanionClass = ACompanionCharacterBase::StaticClass();
	AddExpectedMessage(TEXT("Could not spawn Strong:"), ELogVerbosity::Warning);
	TestFalse(TEXT("No floor causes a safe spawn failure"), Mode->SetupCompanionPair(CompanionController));
	TestNull(TEXT("No companion spawned over empty space"), Mode->StrongCompanion.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionFollowerMovementTest,
	"MechanicTesting.Companions.FollowerMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionFollowerMovementTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	for (bool bAgile : {true, false})
	{
		FTestWorld TestWorld(0.0f);
		TestWorld.Character->ConfigureAgileAbilities(bAgile);
		TestWorld.World->SpawnActor<AAIController>()->Possess(TestWorld.Character);
		TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, 90.0));
		TestWorld.Movement->SetMovementMode(MOVE_Walking);
		TestWorld.Movement->Velocity = FVector::ZeroVector;
#if UE_VERSION_OLDER_THAN(5, 5, 0)
		TestTrue(TEXT("AI path following uses acceleration"), TestWorld.Movement->UseAccelerationForPathFollowing());
#else
		TestTrue(TEXT("AI path following uses acceleration"), TestWorld.Movement->GetNavMovementProperties()->bUseAccelerationForPaths);
#endif
		TestWorld.Movement->RequestPathMove(FVector::ForwardVector);
		TestWorld.Step(false);
		TestFalse(TEXT("AI movement supplies acceleration for the animation"), TestWorld.Movement->GetCurrentAcceleration().IsNearlyZero());
		TestTrue(TEXT("AI movement produces forward velocity"), TestWorld.Movement->Velocity.X > 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionAirborneFollowTest,
	"MechanicTesting.Companions.AirborneFollowing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionAirborneFollowTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	FTestWorld TestWorld(0.0f);
	TestWorld.Player->UnPossess();
	ACompanionSwitchPlayerController* CompanionController = TestWorld.World->SpawnActor<ACompanionSwitchPlayerController>();
	CompanionController->Possess(TestWorld.Character);
	ACompanionCharacterBase* Follower = TestWorld.World->SpawnActor<ACompanionCharacterBase>(
		FVector(-1000.0, 0.0, 90.0), FRotator::ZeroRotator);
	CompanionController->SetCompanionPair(TestWorld.Character, Follower);
	FVector Ground;
	TestTrue(TEXT("Ground is found while leader is airborne"), CompanionController->FindGroundBelowLeader(TestWorld.Character, Follower, Ground));
	TestTrue(TEXT("Follow location is the floor, not the airborne capsule"), FMath::IsNearlyZero(Ground.Z));
	TestWorld.Character->SetActorLocation(FVector(400.0, 0.0, 900.0));
	TestTrue(TEXT("Ground tracking continues as the airborne leader moves"), CompanionController->FindGroundBelowLeader(TestWorld.Character, Follower, Ground));
	TestTrue(TEXT("Ground target follows horizontal position"), FMath::IsNearlyEqual(Ground.X, 400.0));
	TestWorld.AddBox(FVector(400.0, 0.0, 250.0), FVector(100.0, 100.0, 50.0));
	TestTrue(TEXT("Nearest platform beneath the leader is found"), CompanionController->FindGroundBelowLeader(TestWorld.Character, Follower, Ground));
	TestTrue(TEXT("Platform top is used rather than the floor below it"), FMath::IsNearlyEqual(Ground.Z, 300.0));
	TestWorld.Character->SetActorLocation(FVector(400.0, 0.0, 3000.0));
	TestFalse(TEXT("Ground search is bounded"), CompanionController->FindGroundBelowLeader(TestWorld.Character, Follower, Ground));

	// Seed a previously validated target: this fixture deliberately has no baked NavMesh.
	ATargetPoint* Target = TestWorld.World->SpawnActor<ATargetPoint>(FVector(400.0, 0.0, 300.0), FRotator::ZeroRotator);
	CompanionController->GroundFollowTarget = Target;
	CompanionController->GroundTargetLeader = TestWorld.Character;
	CompanionController->bHasGroundFollowTarget = true;
	AAIController* FollowerAI = CastChecked<AAIController>(Follower->GetController());
	Follower->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	UPathFollowingComponent* PathFollowing = FollowerAI->GetPathFollowingComponent();
	const FNavPathSharedPtr Path = MakeShared<FNavigationPath, ESPMode::ThreadSafe>(
		TArray<FVector>{Follower->GetActorLocation(), Target->GetActorLocation()});
	FAIMoveRequest Request(Target);
	Request.SetAcceptanceRadius(250.0f);
	const FAIRequestID MoveId = FollowerAI->RequestMove(Request, Path);
	TestWorld.Character->SetActorLocation(FVector(400.0, 0.0, 900.0));
	for (int32 Refresh = 0; Refresh < 4; ++Refresh) CompanionController->Tick(0.3f);
	TestTrue(TEXT("No navigation projection retains the last valid target"), Target->GetActorLocation().Equals(FVector(400.0, 0.0, 300.0)));
	TestTrue(TEXT("Follow refresh preserves the active request"), PathFollowing->GetCurrentRequestId() == MoveId);
	TestTrue(TEXT("Follower remains moving while leader is airborne"), FollowerAI->GetMoveStatus() == EPathFollowingStatus::Moving);

	TestWorld.Character->SetActorLocation(FVector(10000.0, 0.0, 900.0));
	TestFalse(TEXT("A jump over empty space has no ground hit"), CompanionController->FindGroundBelowLeader(TestWorld.Character, Follower, Ground));
	CompanionController->Tick(0.3f);
	TestTrue(TEXT("No ground hit retains the last valid target"), Target->GetActorLocation().Equals(FVector(400.0, 0.0, 300.0)));
	PathFollowing->PauseMove(MoveId);
	Follower->SetActorLocation(Target->GetActorLocation() + FVector(0.0, 100.0, Follower->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	CompanionController->Tick(0.3f);
	TestTrue(TEXT("Follow refresh preserves paused traversal even near the target"), FollowerAI->GetMoveStatus() == EPathFollowingStatus::Paused);
	PathFollowing->ResumeMove(MoveId);
	CompanionController->Tick(0.3f);
	TestTrue(TEXT("Stopping distance uses the ground target, not the airborne leader"), FollowerAI->GetMoveStatus() == EPathFollowingStatus::Idle);
	CompanionController->SwitchControlledCharacter();
	TestFalse(TEXT("Switching clears the previous leader's cached target"), CompanionController->bHasGroundFollowTarget);
	TestTrue(TEXT("Ground tracking now belongs to the newly controlled character"), CompanionController->GroundTargetLeader.Get() == Follower);
	CompanionController->EndPlay(EEndPlayReason::Destroyed);
	CompanionController->Destroy();
	TestTrue(TEXT("The transient follow target is cleaned up with the controller"), Target->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionJumpPlanTest,
	"MechanicTesting.Companions.JumpLinkPlanning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionJumpPlanTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	FTestWorld TestWorld(0.0f);
	TestWorld.Movement->SetMovementMode(MOVE_Walking);
	TestWorld.Movement->JumpZVelocity = 700.0f;
	TestWorld.Movement->MaxWalkSpeed = 500.0f;
	TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, 90.0));
	TestWorld.AddBox(FVector(500.0, 0.0, 50.0), FVector(200.0, 300.0, 50.0));
	ACompanionJumpNavLink* Link = TestWorld.World->SpawnActor<ACompanionJumpNavLink>();
	TestTrue(TEXT("Native link is smart and relevant"), Link->bSmartLinkIsRelevant);
	TestEqual(TEXT("No simple link can bypass jump execution"), Link->PointLinks.Num(), 0);
	FVector Start;
	FVector End;
	ENavLinkDirection::Type Direction;
	Link->GetSmartLinkComp()->GetLinkData(Start, End, Direction);
	TestTrue(TEXT("Native start widget configures the smart link"), Start.Equals(Link->StartPoint));
	TestTrue(TEXT("Native end widget configures the smart link"), End.Equals(Link->EndPoint));
	TestTrue(TEXT("Both directions are enabled by default"), Direction == ENavLinkDirection::BothWays);
	FVector Velocity;
	FVector Landing;
	FString Reason;
	for (bool bAgile : {true, false})
	{
		TestWorld.Character->ConfigureAgileAbilities(bAgile);
		TestTrue(TEXT("A reachable upward jump is planned for either role"), Link->BuildJumpPlan(TestWorld.Character, FVector(500.0, 0.0, 100.0), Velocity, Landing, Reason));
		TestTrue(TEXT("The normal vertical jump speed is respected"), FMath::IsNearlyEqual(Velocity.Z, 700.0));
		TestTrue(TEXT("The normal horizontal speed limit is respected"), Velocity.Size2D() <= 500.0);
	}
	TestWorld.Character->SetActorLocation(FVector(500.0, 0.0, 190.0));
	TestTrue(TEXT("The same link can plan the return drop"), Link->BuildJumpPlan(TestWorld.Character, FVector::ZeroVector, Velocity, Landing, Reason));
	TestTrue(TEXT("A downward route has no launch impulse"), Velocity.IsNearlyZero());
	TestWorld.Character->SetActorLocation(FVector(0.0, 0.0, 90.0));
	TestWorld.AddBox(FVector(1000.0, 0.0, 350.0), FVector(150.0, 300.0, 50.0));
	TestFalse(TEXT("An unreachable height is rejected"), Link->BuildJumpPlan(TestWorld.Character, FVector(1000.0, 0.0, 400.0), Velocity, Landing, Reason));
	TestFalse(TEXT("An unreachable horizontal distance is rejected"), Link->BuildJumpPlan(TestWorld.Character, FVector(4000.0, 0.0, 0.0), Velocity, Landing, Reason));
	TestFalse(TEXT("Missing landing ground is rejected"), Link->BuildJumpPlan(TestWorld.Character, FVector(10000.0, 0.0, 0.0), Velocity, Landing, Reason));
	TestWorld.AddBox(FVector(250.0, 0.0, 250.0), FVector(20.0, 300.0, 250.0));
	TestFalse(TEXT("A wall blocking the full capsule arc is rejected"), Link->BuildJumpPlan(TestWorld.Character, FVector(500.0, 0.0, 100.0), Velocity, Landing, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionJumpTraversalTest,
	"MechanicTesting.Companions.JumpLinkTraversal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCompanionJumpTraversalTest::RunTest(const FString& Parameters)
{
	using namespace CompanionWallRunTests;
	FTestWorld TestWorld(0.0f);
	TestWorld.AddBox(FVector(500.0, 0.0, 50.0), FVector(200.0, 300.0, 50.0));
	TestWorld.Player->UnPossess();
	AAIController* AI = TestWorld.World->SpawnActor<AAIController>();
	AI->Possess(TestWorld.Character);
	TestWorld.Movement->JumpZVelocity = 700.0f;
	TestWorld.Movement->MaxWalkSpeed = 500.0f;
	TestWorld.Movement->AirControl = 0.35f;
	ACompanionJumpNavLink* Link = TestWorld.World->SpawnActor<ACompanionJumpNavLink>();
	Link->DispatchBeginPlay();
	const auto StartJump = [&](bool bReturn = false)
	{
		const FVector Destination = bReturn ? FVector::ZeroVector : FVector(500.0, 0.0, 100.0);
		TestWorld.Character->SetActorLocation(bReturn ? FVector(500.0, 0.0, 190.0) : FVector(0.0, 0.0, 90.0));
		TestWorld.Movement->SetMovementMode(MOVE_Walking);
		TestWorld.Movement->Velocity = FVector::ZeroVector;
		if (AI->GetPawn() != TestWorld.Character) AI->Possess(TestWorld.Character);
		FAIMoveRequest Request(Destination);
		Request.SetAcceptanceRadius(50.0f);
		AI->RequestMove(Request, MakeShared<FNavigationPath, ESPMode::ThreadSafe>(
			TArray<FVector>{TestWorld.Movement->GetActorFeetLocation(), Destination}));
		// Exercise the actual component-to-proxy callback without a baked NavMesh.
		Link->GetSmartLinkComp()->OnLinkMoveStarted(AI->GetPathFollowingComponent(), Destination);
	};
	StartJump();
	TestTrue(TEXT("Smart-link callback starts traversal"), Link->Traversals.Contains(TestWorld.Character));
	TestTrue(TEXT("Path following is paused during the jump"), AI->GetMoveStatus() == EPathFollowingStatus::Paused);
	for (int32 Step = 0; Step < 240 && !Link->Traversals.IsEmpty(); ++Step)
	{
		TestWorld.Step(false);
		Link->Tick(1.0f / 60.0f);
	}
	TestTrue(TEXT("Landing completes traversal"), Link->Traversals.IsEmpty());
	TestTrue(TEXT("Character reaches the upper landing"), FVector::Dist(TestWorld.Movement->GetActorFeetLocation(), FVector(500.0, 0.0, 100.0)) <= Link->LandingTolerance);
	TestTrue(TEXT("Normal air control is restored after landing"), FMath::IsNearlyEqual(TestWorld.Movement->AirControl, 0.35f));
	TestTrue(TEXT("Path following is no longer paused"), AI->GetMoveStatus() != EPathFollowingStatus::Paused);
	Link->GetSmartLinkComp()->OnLinkMoveStarted(AI->GetPathFollowingComponent(), FVector(500.0, 0.0, 100.0));
	TestTrue(TEXT("Repeating the completed link does not start a second jump"), Link->Traversals.IsEmpty());
	TestWorld.Step(false);
	TestTrue(TEXT("No jump is queued after arrival"), TestWorld.Movement->IsMovingOnGround());
	StartJump(true);
	TestTrue(TEXT("The reverse route still starts after arrival"), Link->Traversals.Contains(TestWorld.Character));
	for (int32 Step = 0; Step < 240 && !Link->Traversals.IsEmpty(); ++Step)
	{
		TestWorld.Step(false);
		Link->Tick(1.0f / 60.0f);
		TestTrue(TEXT("Dropping never adds upward velocity"), TestWorld.Movement->Velocity.Z <= KINDA_SMALL_NUMBER);
	}
	TestTrue(TEXT("Return drop completes traversal"), Link->Traversals.IsEmpty());
	TestTrue(TEXT("Return drop reaches the lower landing"), TestWorld.Movement->GetActorFeetLocation().Size() <= Link->LandingTolerance);
	Link->GetSmartLinkComp()->OnLinkMoveStarted(AI->GetPathFollowingComponent(), FVector::ZeroVector);
	TestTrue(TEXT("Repeating the completed drop does not launch again"), Link->Traversals.IsEmpty());
	StartJump();
	AI->UnPossess();
	TestWorld.Player->Possess(TestWorld.Character);
	Link->Tick(1.0f / 60.0f);
	TestTrue(TEXT("Changing possession cancels link ownership"), Link->Traversals.IsEmpty());
	TestTrue(TEXT("Player regains normal air control"), FMath::IsNearlyEqual(TestWorld.Movement->AirControl, 0.35f));
	TestEqual(TEXT("Player possession is not disturbed"), TestWorld.Character->GetController(), static_cast<AController*>(TestWorld.Player));
	TestWorld.Player->UnPossess();
	StartJump();
	AddExpectedMessage(TEXT("the traversal timed out"), ELogVerbosity::Warning);
	Link->Tick(Link->TraversalTimeout + 1.0f);
	TestTrue(TEXT("Timeout clears traversal"), Link->Traversals.IsEmpty());
	TestTrue(TEXT("Timeout aborts the AI move"), AI->GetMoveStatus() == EPathFollowingStatus::Idle);
	TestTrue(TEXT("Timeout restores movement settings"), FMath::IsNearlyEqual(TestWorld.Movement->AirControl, 0.35f));
	StartJump();
	Link->EndPlay(EEndPlayReason::Destroyed);
	TestTrue(TEXT("Removing the link clears traversal"), Link->Traversals.IsEmpty());
	TestTrue(TEXT("Removing the link restores movement settings"), FMath::IsNearlyEqual(TestWorld.Movement->AirControl, 0.35f));
	return true;
}

#endif

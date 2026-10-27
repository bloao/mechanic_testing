#if WITH_DEV_AUTOMATION_TESTS

#include "mechanic_testing/CompanionCharacterBase.h"
#include "mechanic_testing/CompanionCharacterMovementComponent.h"
#include "mechanic_testing/CompanionGameMode.h"
#include "mechanic_testing/CompanionSwitchPlayerController.h"
#include "mechanic_testing/BreakableActor.h"
#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

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

#endif

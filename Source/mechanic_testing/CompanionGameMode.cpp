#include "CompanionGameMode.h"

#include "CompanionCharacterBase.h"
#include "CompanionPlayableCharacter.h"
#include "CompanionSwitchPlayerController.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

ACompanionGameMode::ACompanionGameMode()
{
	DefaultPawnClass = ACompanionPlayableCharacter::StaticClass();
	StrongCompanionClass = ACompanionPlayableCharacter::StaticClass();
	PlayerControllerClass = ACompanionSwitchPlayerController::StaticClass();
}

void ACompanionGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	PreparePlacedPlayerPawns();
}

void ACompanionGameMode::PreparePlacedPlayerPawns()
{
	// Do not delete, hide or move authored pawns. Stop an old auto-possessed
	// template player from taking input away from the native companion pair.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->AutoPossessPlayer == EAutoReceiveInput::Player0)
		{
			It->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			It->AutoPossessAI = EAutoPossessAI::Disabled;
			UE_LOG(LogTemp, Display, TEXT("Companion setup: disabled legacy auto-possession on %s for this play session."), *It->GetName());
		}
	}
}

void ACompanionGameMode::StartPlay()
{
	EnsureNavigationBounds();
	Super::StartPlay();
	UE_LOG(LogTemp, Display, TEXT("Companion mechanics active in %s: %s / %s; Agile=%s Strong=%s."),
		*GetWorld()->GetMapName(), *GetNameSafe(DefaultPawnClass.Get()), *GetNameSafe(PlayerControllerClass.Get()),
		*GetNameSafe(AgileCompanion), *GetNameSafe(StrongCompanion));
}

ANavMeshBoundsVolume* ACompanionGameMode::EnsureNavigationBounds()
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		FNavigationSystem::AddNavigationSystemToWorld(*GetWorld(), FNavigationSystemRunMode::GameMode);
		Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	}
	if (!Navigation) return nullptr;
	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
	{
		// Keep the level designer's coverage and links exactly as authored.
		Navigation->OnNavigationBoundsUpdated(*It);
		return *It;
	}
	if (!bCreateNavigationBoundsIfMissing) return nullptr;
	FBox GeometryBounds(ForceInit);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<APawn>() || It->IsA<AVolume>() || It->IsA<ANavigationData>()) continue;
		TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->IsRegistered() && Primitive->CanEverAffectNavigation()
				&& Primitive->IsQueryCollisionEnabled() && !Primitive->IsSimulatingPhysics()
				&& Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				GeometryBounds += Primitive->Bounds.GetBox();
			}
		}
	}
	if (!GeometryBounds.IsValid) return nullptr;
	GeometryBounds = GeometryBounds.ExpandBy(FVector(100.0, 100.0, 300.0));
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	ANavMeshBoundsVolume* Bounds = GetWorld()->SpawnActor<ANavMeshBoundsVolume>(
		GeometryBounds.GetCenter(), FRotator::ZeroRotator, Parameters);
	if (!Bounds) return nullptr;
	UBoxComponent* Coverage = NewObject<UBoxComponent>(Bounds, TEXT("CompanionNavigationCoverage"));
	Coverage->SetupAttachment(Bounds->GetRootComponent());
	Coverage->SetMobility(EComponentMobility::Static);
	Coverage->InitBoxExtent(GeometryBounds.GetExtent());
	Coverage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Coverage->SetCanEverAffectNavigation(false);
	Coverage->RegisterComponent();
	Bounds->SetActorHiddenInGame(true);
	Navigation->OnNavigationBoundsUpdated(Bounds);
	UE_LOG(LogTemp, Display, TEXT("Companion setup: added transient navigation bounds; authored level assets are unchanged."));
	return Bounds;
}

void ACompanionGameMode::RestartPlayer(AController* NewPlayer)
{
	if (IsValid(NewPlayer) && IsValid(NewPlayer->GetPawn())
		&& !NewPlayer->GetPawn()->IsA<ACompanionPlayableCharacter>())
	{
		NewPlayer->UnPossess();
	}
	Super::RestartPlayer(NewPlayer);
	if (ACompanionSwitchPlayerController* CompanionController = Cast<ACompanionSwitchPlayerController>(NewPlayer))
	{
		SetupCompanionPair(CompanionController);
	}
}

bool ACompanionGameMode::SetupCompanionPair(ACompanionSwitchPlayerController* PlayerController)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return false;
	}
	if (!IsValid(AgileCompanion))
	{
		AgileCompanion = Cast<ACompanionCharacterBase>(PlayerController->GetPawn());
	}
	if (!IsValid(AgileCompanion))
	{
		UE_LOG(LogTemp, Warning, TEXT("CompanionGameMode needs a CompanionPlayableCharacter pawn. Disable Auto Possess Player on old placed template characters."));
		return false;
	}
	// Agile must retain its movement abilities even if nearby geometry prevents
	// the second character spawning. Pair creation is not an ability prerequisite.
	AgileCompanion->ConfigureAgileAbilities(true);
	if (!IsValid(StrongCompanion))
	{
		StrongCompanion = SpawnStrongCompanion();
	}
	if (!IsValid(StrongCompanion))
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not spawn Strong: leave free, walkable floor around PlayerStart and check Strong Companion Class."));
		return false;
	}
	AgileCompanion->Tags.AddUnique(TEXT("CompanionCharacter"));
	StrongCompanion->Tags.AddUnique(TEXT("CompanionCharacter"));
#if WITH_EDITOR
	AgileCompanion->SetActorLabel(TEXT("Agile"));
	StrongCompanion->SetActorLabel(TEXT("Strong"));
#endif
	PlayerController->SetCompanionPair(AgileCompanion, StrongCompanion);
	return true;
}

ACompanionCharacterBase* ACompanionGameMode::SpawnStrongCompanion()
{
	if (!StrongCompanionClass || !IsValid(AgileCompanion))
	{
		return nullptr;
	}
	const ACompanionCharacterBase* Defaults = StrongCompanionClass.GetDefaultObject();
	const float HalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Right = AgileCompanion->GetActorRightVector();
	const FVector Forward = AgileCompanion->GetActorForwardVector();
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(CompanionSpawnFloor), false, AgileCompanion);
	FCollisionObjectQueryParams FloorObjects;
	FloorObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	FloorObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const float MinSeparation = AgileCompanion->GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.0f;
	for (float Scale : {1.0f, 0.75f, 0.5f, 0.0f})
	{
		const float Distance = FMath::Max(CompanionSpawnDistance * Scale, MinSeparation);
		for (const FVector& Direction : {Right, -Right, -Forward, Forward,
			(Right + Forward).GetSafeNormal(), (Right - Forward).GetSafeNormal(),
			(-Right + Forward).GetSafeNormal(), (-Right - Forward).GetSafeNormal()})
		{
			const FVector Candidate = AgileCompanion->GetActorLocation() + Direction * Distance;
			FHitResult Floor;
			if (!GetWorld()->LineTraceSingleByObjectType(Floor, Candidate + FVector::UpVector * 200.0,
				Candidate - FVector::UpVector * 600.0, FloorObjects, Query)
				|| Floor.ImpactNormal.Z < Defaults->GetCharacterMovement()->GetWalkableFloorZ()
				|| !Floor.GetComponent() || Floor.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
				|| Floor.GetComponent()->IsSimulatingPhysics() || !Floor.GetComponent()->GetComponentVelocity().IsNearlyZero(1.0f))
			{
				continue;
			}
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			const FVector SpawnLocation = Floor.ImpactPoint + FVector::UpVector * (HalfHeight + 2.0f);
			if (ACompanionCharacterBase* SpawnedCompanion = GetWorld()->SpawnActor<ACompanionCharacterBase>(
				StrongCompanionClass, SpawnLocation, AgileCompanion->GetActorRotation(), Parameters))
			{
				return SpawnedCompanion;
			}
		}
	}
	return nullptr;
}

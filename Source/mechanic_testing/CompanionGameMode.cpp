#include "CompanionGameMode.h"

#include "CompanionCharacterBase.h"
#include "CompanionPlayableCharacter.h"
#include "CompanionSwitchPlayerController.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

ACompanionGameMode::ACompanionGameMode()
{
	DefaultPawnClass = ACompanionPlayableCharacter::StaticClass();
	StrongCompanionClass = ACompanionPlayableCharacter::StaticClass();
	PlayerControllerClass = ACompanionSwitchPlayerController::StaticClass();
}

void ACompanionGameMode::RestartPlayer(AController* NewPlayer)
{
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
	for (const FVector& Direction : {Right, -Right, -Forward, Forward})
	{
		const FVector Candidate = AgileCompanion->GetActorLocation() + Direction * CompanionSpawnDistance;
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByObjectType(Floor, Candidate + FVector::UpVector * 200.0,
			Candidate - FVector::UpVector * 600.0, FCollisionObjectQueryParams(ECC_WorldStatic), Query)
			|| Floor.ImpactNormal.Z < Defaults->GetCharacterMovement()->GetWalkableFloorZ()
			|| !Floor.GetComponent() || Floor.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
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
	return nullptr;
}

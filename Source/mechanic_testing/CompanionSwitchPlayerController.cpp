#include "CompanionSwitchPlayerController.h"

#include "AIController.h"
#include "CompanionCharacterBase.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ACompanionSwitchPlayerController::ACompanionSwitchPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	bAutoDiscoverCharacters = true;
	CompanionTag = TEXT("CompanionCharacter");
	SwitchCharacterKey = EKeys::Tab;
	InputMappingPriority = 0;
	FollowAcceptanceRadius = 250.0f;
	FollowRefreshInterval = 0.25f;
	AgileCompanionIndex = 0;
	AgileJumpMaxCount = 2;
	bLogCompanionSetup = false;

	ControlledCompanionIndex = INDEX_NONE;
	TimeSinceLastFollowUpdate = 0.0f;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingContextFinder(
		TEXT("/Game/Input/IMC_Default.IMC_Default"));
	DefaultInputMappingContext = MappingContextFinder.Object;
}

void ACompanionSwitchPlayerController::BeginPlay()
{
	Super::BeginPlay();

	InitializeCompanions();

	if (DefaultInputMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(DefaultInputMappingContext, InputMappingPriority);
		}
	}
}

void ACompanionSwitchPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (WasInputKeyJustPressed(SwitchCharacterKey))
	{
		SwitchControlledCharacter();
	}

	TimeSinceLastFollowUpdate += DeltaSeconds;
	if (TimeSinceLastFollowUpdate >= FollowRefreshInterval)
	{
		TimeSinceLastFollowUpdate = 0.0f;
		UpdateFollowerMove();
	}
}

void ACompanionSwitchPlayerController::InitializeCompanions()
{
	EnsureTwoCompanions();

	if (CompanionCharacters.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Companion setup needs two characters. Found %d."), CompanionCharacters.Num());
		return;
	}

	ApplyCompanionAbilitySettings();

	const int32 ExistingPawnIndex = CompanionCharacters.IndexOfByKey(Cast<ACharacter>(GetPawn()));
	PossessCompanionAtIndex(ExistingPawnIndex != INDEX_NONE ? ExistingPawnIndex : 0);
}

void ACompanionSwitchPlayerController::SwitchControlledCharacter()
{
	if (CompanionCharacters.Num() < 2)
	{
		InitializeCompanions();
	}

	if (CompanionCharacters.Num() < 2)
	{
		return;
	}

	const int32 NextIndex = ControlledCompanionIndex == 0 ? 1 : 0;
	PossessCompanionAtIndex(NextIndex);
}

ACharacter* ACompanionSwitchPlayerController::GetControlledCompanion() const
{
	return GetCompanionAtIndex(ControlledCompanionIndex);
}

ACharacter* ACompanionSwitchPlayerController::GetFollowerCompanion() const
{
	if (CompanionCharacters.Num() < 2 || ControlledCompanionIndex == INDEX_NONE)
	{
		return nullptr;
	}

	return GetCompanionAtIndex(ControlledCompanionIndex == 0 ? 1 : 0);
}

void ACompanionSwitchPlayerController::EnsureTwoCompanions()
{
	CompanionCharacters.RemoveAll([](const ACharacter* CompanionPawn)
	{
		return !IsValid(CompanionPawn);
	});

	if (!bAutoDiscoverCharacters || CompanionCharacters.Num() >= 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* CompanionPawn = *It;
		if (!IsValid(CompanionPawn) || CompanionCharacters.Contains(CompanionPawn))
		{
			continue;
		}

		if (!CompanionTag.IsNone() && !CompanionPawn->ActorHasTag(CompanionTag))
		{
			continue;
		}

		CompanionCharacters.Add(CompanionPawn);
		if (CompanionCharacters.Num() >= 2)
		{
			break;
		}
	}

	if (CompanionCharacters.Num() < 2)
	{
		if (ACharacter* CurrentCharacter = Cast<ACharacter>(GetPawn()))
		{
			CompanionCharacters.AddUnique(CurrentCharacter);
		}
	}

	if (bLogCompanionSetup)
	{
		UE_LOG(LogTemp, Log, TEXT("Companion setup found %d characters."), CompanionCharacters.Num());
	}
}

void ACompanionSwitchPlayerController::ApplyCompanionAbilitySettings()
{
	const int32 ValidAgileIndex = FMath::Clamp(AgileCompanionIndex, 0, 1);
	for (int32 Index = 0; Index < CompanionCharacters.Num(); ++Index)
	{
		if (ACharacter* CompanionPawn = CompanionCharacters[Index])
		{
			const bool bIsAgile = Index == ValidAgileIndex;
			if (ACompanionCharacterBase* Companion = Cast<ACompanionCharacterBase>(CompanionPawn))
			{
				Companion->ConfigureAgileAbilities(bIsAgile, AgileJumpMaxCount);
			}
			else
			{
				CompanionPawn->JumpMaxCount = bIsAgile ? FMath::Max(AgileJumpMaxCount, 2) : 1;
				UE_LOG(LogTemp, Warning, TEXT("%s needs CompanionCharacterBase as its parent for wall running."), *CompanionPawn->GetName());
			}
		}
	}
}

void ACompanionSwitchPlayerController::PossessCompanionAtIndex(int32 NewIndex)
{
	ACharacter* NewControlledCharacter = GetCompanionAtIndex(NewIndex);
	if (!NewControlledCharacter)
	{
		return;
	}

	ACharacter* OldControlledCharacter = GetControlledCompanion();
	if (OldControlledCharacter && OldControlledCharacter != NewControlledCharacter)
	{
		UnPossess();
		EnsureAIController(OldControlledCharacter);
	}

	ControlledCompanionIndex = NewIndex;
	Possess(NewControlledCharacter);

	UpdateFollowerMove();
}

void ACompanionSwitchPlayerController::UpdateFollowerMove()
{
	ACharacter* Leader = GetControlledCompanion();
	ACharacter* Follower = GetFollowerCompanion();
	if (!Leader || !Follower)
	{
		return;
	}

	AAIController* FollowerController = EnsureAIController(Follower);
	if (!FollowerController)
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(Leader->GetActorLocation(), Follower->GetActorLocation());
	const float StopDistance = FMath::Max(FollowAcceptanceRadius, 1.0f);
	if (DistanceSquared <= FMath::Square(StopDistance))
	{
		FollowerController->StopMovement();
		return;
	}

	FollowerController->MoveToActor(Leader, FollowAcceptanceRadius, true, true, true, nullptr, true);
}

AAIController* ACompanionSwitchPlayerController::EnsureAIController(ACharacter* CompanionPawn) const
{
	if (!CompanionPawn)
	{
		return nullptr;
	}

	if (AAIController* ExistingController = Cast<AAIController>(CompanionPawn->GetController()))
	{
		return ExistingController;
	}

	CompanionPawn->SpawnDefaultController();
	return Cast<AAIController>(CompanionPawn->GetController());
}

ACharacter* ACompanionSwitchPlayerController::GetCompanionAtIndex(int32 Index) const
{
	return CompanionCharacters.IsValidIndex(Index) ? CompanionCharacters[Index] : nullptr;
}

#include "CompanionCharacterBase.h"

#include "CompanionCharacterMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Components/InputComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "SmashableInterface.h"

ACompanionCharacterBase::ACompanionCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCompanionCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bCanDoubleJump = false;
	MaxJumpCount = 2;
	bCanSmash = false;
	SmashRange = 180.0f;
	SmashRadius = 90.0f;
	SmashCooldown = 0.4f;
	LastSmashTime = -DBL_MAX;

	ApplyJumpSettings();
}

void ACompanionCharacterBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyJumpSettings();
}

void ACompanionCharacterBase::ConfigureAgileAbilities(bool bIsAgile, int32 AgileJumpCount)
{
	bCanDoubleJump = bIsAgile;
	bCanSmash = !bIsAgile;
	MaxJumpCount = FMath::Max(AgileJumpCount, 2);
	ApplyJumpSettings();
	if (UCompanionCharacterMovementComponent* Movement = Cast<UCompanionCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->SetWallRunEnabled(bIsAgile);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("WallRun [%s]: movement component is %s, not CompanionCharacterMovementComponent. Recompile the character Blueprint after restarting the editor."),
			*GetName(), *GetNameSafe(GetCharacterMovement()->GetClass()));
	}
}

void ACompanionCharacterBase::Jump()
{
	if (bJumpInputHeld)
	{
		return;
	}
	bJumpInputHeld = true;
	if (UCompanionCharacterMovementComponent* Movement = Cast<UCompanionCharacterMovementComponent>(GetCharacterMovement()))
	{
		// Unreal clears bPressedJump itself; track the physical press until StopJumping.
		if (Movement->JumpOffWall())
		{
			Super::StopJumping();
			return;
		}
	}
	Super::Jump();
}

void ACompanionCharacterBase::StopJumping()
{
	bJumpInputHeld = false;
	Super::StopJumping();
}

void ACompanionCharacterBase::UnPossessed()
{
	StopJumping();
	if (UCompanionCharacterMovementComponent* Movement = Cast<UCompanionCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->StopWallRun();
	}
	Super::UnPossessed();
}

void ACompanionCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (SmashInputAction)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
		{
			EnhancedInput->BindAction(SmashInputAction, ETriggerEvent::Started, this, &ACompanionCharacterBase::HandleSmashInput);
		}
	}
}

bool ACompanionCharacterBase::PerformSmash()
{
	if (!CanPerformSmash())
	{
		return false;
	}

	LastSmashTime = GetWorld()->GetTimeSeconds();

	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorForwardVector() * SmashRange;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CompanionSmash), false, this);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(SmashRadius),
		QueryParams);

	bool bHitBreakable = false;
	TSet<AActor*> ProcessedActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target)
			|| ProcessedActors.Contains(Target)
			|| !Target->GetClass()->ImplementsInterface(USmashableInterface::StaticClass()))
		{
			continue;
		}
		ProcessedActors.Add(Target);

		if (ISmashableInterface::Execute_CanBeSmashedBy(Target, this))
		{
			ISmashableInterface::Execute_ReceiveSmash(Target, this);
			bHitBreakable = true;
		}
	}

	BP_OnSmashPerformed(bHitBreakable);
	return bHitBreakable;
}

bool ACompanionCharacterBase::CanPerformSmash() const
{
	return bCanSmash
		&& GetWorld()
		&& GetWorld()->GetTimeSeconds() - LastSmashTime >= SmashCooldown;
}

void ACompanionCharacterBase::ApplyJumpSettings()
{
	JumpMaxCount = bCanDoubleJump ? FMath::Max(MaxJumpCount, 2) : 1;
}

void ACompanionCharacterBase::HandleSmashInput()
{
	PerformSmash();
}

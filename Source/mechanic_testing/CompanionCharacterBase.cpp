#include "CompanionCharacterBase.h"

#include "CollisionQueryParams.h"
#include "Components/InputComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "SmashableInterface.h"

ACompanionCharacterBase::ACompanionCharacterBase()
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

void ACompanionCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (bCanSmash && SmashInputAction)
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

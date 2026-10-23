#include "BreakableActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ABreakableActor::ABreakableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);

	bIsBreakable = true;
	HitsRequired = 1;
	bHideWhenBroken = true;
	bDisableCollisionWhenBroken = true;
	bDestroyWhenBroken = false;
	DestroyDelay = 0.0f;
	HitCount = 0;
	bBroken = false;
}

bool ABreakableActor::CanBeSmashedBy_Implementation(AActor* SourceActor) const
{
	return bIsBreakable && !bBroken;
}

void ABreakableActor::ReceiveSmash_Implementation(AActor* SourceActor)
{
	if (!CanBeSmashedBy_Implementation(SourceActor))
	{
		return;
	}

	++HitCount;
	BP_OnSmashHit(SourceActor, HitCount, FMath::Max(HitsRequired, 1));

	if (HitCount >= FMath::Max(HitsRequired, 1))
	{
		Break(SourceActor);
	}
}
void ABreakableActor::Break(AActor* SourceActor)
{
	if (bBroken)
	{
		return;
	}

	bBroken = true;
	BP_OnBroken(SourceActor);

	if (bDisableCollisionWhenBroken)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (bHideWhenBroken)
	{
		Mesh->SetVisibility(false, true);
	}

	if (bDestroyWhenBroken)
	{
		if (DestroyDelay > 0.0f)
		{
			SetLifeSpan(DestroyDelay);
		}
		else
		{
			Destroy();
		}
	}
}

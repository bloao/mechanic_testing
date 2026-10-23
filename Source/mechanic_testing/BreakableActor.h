#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SmashableInterface.h"
#include "BreakableActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ABreakableActor : public AActor, public ISmashableInterface
{
	GENERATED_BODY()

public:
	ABreakableActor();

	virtual bool CanBeSmashedBy_Implementation(AActor* SourceActor) const override;
	virtual void ReceiveSmash_Implementation(AActor* SourceActor) override;

	UFUNCTION(BlueprintCallable, Category = "Breakable")
	void Break(AActor* SourceActor);

	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const { return bBroken; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	bool bIsBreakable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable", meta = (ClampMin = "1", UIMin = "1"))
	int32 HitsRequired;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Result")
	bool bHideWhenBroken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Result")
	bool bDisableCollisionWhenBroken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Result")
	bool bDestroyWhenBroken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Result", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", EditCondition = "bDestroyWhenBroken"))
	float DestroyDelay;

	UFUNCTION(BlueprintImplementableEvent, Category = "Breakable", meta = (DisplayName = "On Smash Hit"))
	void BP_OnSmashHit(AActor* SourceActor, int32 CurrentHits, int32 RequiredHits);

	UFUNCTION(BlueprintImplementableEvent, Category = "Breakable", meta = (DisplayName = "On Broken"))
	void BP_OnBroken(AActor* SourceActor);

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Breakable")
	int32 HitCount;

	UPROPERTY(VisibleInstanceOnly, Category = "Breakable")
	bool bBroken;
};

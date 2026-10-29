#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Navigation/NavLinkProxy.h"
#include "CompanionJumpNavLink.generated.h"

class AAIController;
class ACharacter;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ACompanionJumpNavLink : public ANavLinkProxy
{
	GENERATED_BODY()

public:
	ACompanionJumpNavLink(const FObjectInitializer& ObjectInitializer);
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Link", meta = (MakeEditWidget = true))
	FVector StartPoint = FVector(-150.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Link", meta = (MakeEditWidget = true))
	FVector EndPoint = FVector(150.0, 0.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Link")
	TEnumAsByte<ENavLinkDirection::Type> Direction = ENavLinkDirection::BothWays;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Link", meta = (ClampMin = "1.0", Units = "s"))
	float TraversalTimeout = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Link", meta = (ClampMin = "10.0", Units = "cm"))
	float LandingTolerance = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Link")
	bool bDrawJumpDebug = false;

private:
	friend class FCompanionJumpPlanTest;
	friend class FCompanionJumpTraversalTest;

	struct FTraversal
	{
		TWeakObjectPtr<AAIController> Controller;
		FAIRequestID RequestId;
		FVector LandingPoint;
		float AirControl = 0.0f;
		float FallingFriction = 0.0f;
		float FallingBraking = 0.0f;
		float Elapsed = 0.0f;
		bool bWasFalling = false;
		bool bDrop = false;
	};

	TMap<TWeakObjectPtr<ACharacter>, FTraversal> Traversals;
	double LastWarningTime = -DBL_MAX;

	UFUNCTION()
	void HandleSmartLinkReached(AActor* Agent, const FVector& Destination);

	bool BuildJumpPlan(ACharacter* CompanionPawn, const FVector& Destination,
		FVector& Velocity, FVector& LandingPoint, FString& FailureReason) const;
	void FinishTraversal(TWeakObjectPtr<ACharacter> CompanionPawn, bool bSuccess);
	void WarnRejected(const FString& Reason);
};

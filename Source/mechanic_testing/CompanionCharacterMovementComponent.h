#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CompanionCharacterMovementComponent.generated.h"

UCLASS()
class MECHANIC_TESTING_API UCompanionCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UCompanionCharacterMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "Abilities|Wall Run")
	void SetWallRunEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Abilities|Wall Run")
	bool IsWallRunning() const { return bWallRunning; }

	void StopWallRun();
	bool JumpOffWall();

	virtual float GetMaxSpeed() const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run")
	bool bCanWallRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "1.0", ToolTip = "Horizontal wall-run speed in centimeters per second."))
	float WallRunSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "1.0", ToolTip = "Minimum entry speed in centimeters per second."))
	float MinWallRunSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "0.1", Units = "s"))
	float MaxWallRunDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallRunGravityMultiplier = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "1.0", Units = "cm"))
	float WallDetectionDistance = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "0.0", ToolTip = "Jump-away speed in centimeters per second."))
	float WallJumpAwaySpeed = 450.0f;

protected:
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;

private:
	bool bWallRunning = false;
	bool bWallRunUsedThisAirborne = false;
	float WallRunElapsed = 0.0f;
	FVector WallNormal = FVector::ZeroVector;
	FVector WallRunDirection = FVector::ZeroVector;

	bool FindWall(const FVector& Direction, FHitResult& Hit) const;
	bool HasWallRunInput() const;
	bool HasPlayerControl() const;
	void TryStartWallRun();
	FString GetWallRunDebugStatus() const;
	FString LastWallRunDebugStatus;
};

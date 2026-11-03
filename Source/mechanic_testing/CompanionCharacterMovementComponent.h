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

	/** Wall side relative to travel along the wall, not the camera. False when not wall running. */
	UFUNCTION(BlueprintPure, Category = "Abilities|Wall Run")
	bool IsWallOnRight() const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "0.0", ToolTip = "Vertical speed set on each wall jump, rather than added to existing velocity. Centimeters per second."))
	float WallJumpUpSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Wall Run", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Time after a wall jump in which an opposing wall can refresh the wall run. Forward and sideways momentum are preserved during this transfer."))
	float WallTransferDuration = 1.2f;

protected:
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;

private:
	bool bWallRunning = false;
	bool bWallRunUsedThisAirborne = false;
	bool bWallTransferActive = false;
	float WallTransferTimeRemaining = 0.0f;
	float WallRunElapsed = 0.0f;
	FVector WallNormal = FVector::ZeroVector;
	FVector WallRunDirection = FVector::ZeroVector;

	bool FindWall(const FVector& Direction, FHitResult& Hit) const;
	FVector GetWallSearchDirection() const;
	bool CanCatchWall(const FVector& Normal) const;
	bool HasWallRunInput() const;
	bool HasPlayerControl() const;
	void TryStartWallRun();
	FString GetWallRunDebugStatus() const;
	FString LastWallRunDebugStatus;
};

#pragma once

#include "CoreMinimal.h"
#include "CompanionCharacterBase.h"
#include "InputCoreTypes.h"
#include "CompanionPlayableCharacter.generated.h"

class UAnimInstance;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USkeletalMesh;
class UStaticMeshComponent;
class USpringArmComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ACompanionPlayableCharacter : public ACompanionCharacterBase
{
	GENERATED_BODY()

public:
	ACompanionPlayableCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitializeComponents() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Jump() override;
	virtual void ConfigureAgileAbilities(bool bIsAgile, int32 AgileJumpCount = 2) override;

	UFUNCTION(BlueprintPure, Category = "Container")
	bool IsHoldingContainer() const { return HeldContainer.IsValid(); }

	/** Actual speed in cm/s while holding a container: X is sideways, Y is forward. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Container")
	FVector2D ContainerMoveVelocity = FVector2D::ZeroVector;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<USkeletalMesh> AgileMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<USkeletalMesh> StrongMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	TSoftClassPtr<UAnimInstance> AgileAnimationClass;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	TSoftClassPtr<UAnimInstance> StrongAnimationClass;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> MouseLookMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	FKey SmashKey;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FKey PushContainerKey = EKeys::F;

private:
	TWeakObjectPtr<UStaticMeshComponent> HeldContainer;
	FVector ContainerForward = FVector::ForwardVector;
	FVector2D ContainerMoveInput = FVector2D::ZeroVector;

	void ApplyAppearance();
	void Move(const FInputActionValue& Value);
	void ClearContainerMoveInput();
	void MoveHeldContainer(float DeltaSeconds);
	void Look(const FInputActionValue& Value);
	void Smash();
	void ToggleContainerGrab();
	void ReleaseContainer();
};

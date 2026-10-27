#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CompanionCharacterBase.generated.h"

class UInputAction;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ACompanionCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ACompanionCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Jump() override;
	virtual void StopJumping() override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	virtual void ConfigureAgileAbilities(bool bIsAgile, int32 AgileJumpCount = 2);

	UFUNCTION(BlueprintCallable, Category = "Abilities|Smash")
	bool PerformSmash();

	UFUNCTION(BlueprintPure, Category = "Abilities|Smash")
	bool CanPerformSmash() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Jump")
	bool bCanDoubleJump;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities|Jump", meta = (ClampMin = "2", UIMin = "2", EditCondition = "bCanDoubleJump"))
	int32 MaxJumpCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities|Smash")
	bool bCanSmash;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities|Smash", meta = (EditCondition = "bCanSmash"))
	TObjectPtr<UInputAction> SmashInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities|Smash", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm", EditCondition = "bCanSmash"))
	float SmashRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities|Smash", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm", EditCondition = "bCanSmash"))
	float SmashRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities|Smash", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", EditCondition = "bCanSmash"))
	float SmashCooldown;

	UFUNCTION(BlueprintImplementableEvent, Category = "Abilities|Smash", meta = (DisplayName = "On Smash Performed"))
	void BP_OnSmashPerformed(bool bHitBreakable);

private:
	double LastSmashTime;
	bool bJumpInputHeld = false;

	void ApplyJumpSettings();
	void HandleSmashInput();
};

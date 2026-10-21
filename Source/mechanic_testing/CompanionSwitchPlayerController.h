#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "CompanionSwitchPlayerController.generated.h"

class AAIController;
class ACharacter;
class UInputMappingContext;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ACompanionSwitchPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACompanionSwitchPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void InitializeCompanions();

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SwitchControlledCharacter();

	UFUNCTION(BlueprintPure, Category = "Companion")
	ACharacter* GetControlledCompanion() const;

	UFUNCTION(BlueprintPure, Category = "Companion")
	ACharacter* GetFollowerCompanion() const;

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Companion", meta = (ToolTip = "Optional explicit pair of characters. If empty, the controller can discover characters by tag."))
	TArray<ACharacter*> CompanionCharacters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion")
	bool bAutoDiscoverCharacters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion", meta = (EditCondition = "bAutoDiscoverCharacters"))
	FName CompanionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Input")
	FKey SwitchCharacterKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Input")
	TObjectPtr<UInputMappingContext> DefaultInputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Input")
	int32 InputMappingPriority;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float FollowAcceptanceRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Follow", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float FollowRefreshInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Abilities", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	int32 AgileCompanionIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Abilities", meta = (ClampMin = "2", UIMin = "2"))
	int32 AgileJumpMaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Debug")
	bool bLogCompanionSetup;

private:
	int32 ControlledCompanionIndex;
	float TimeSinceLastFollowUpdate;

	void EnsureTwoCompanions();
	void ApplyCompanionAbilitySettings();
	void PossessCompanionAtIndex(int32 NewIndex);
	void UpdateFollowerMove();
	AAIController* EnsureAIController(ACharacter* Character) const;
	ACharacter* GetCompanionAtIndex(int32 Index) const;
};

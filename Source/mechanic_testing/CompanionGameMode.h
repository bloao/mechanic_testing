#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CompanionGameMode.generated.h"

class ACompanionCharacterBase;
class ACompanionSwitchPlayerController;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ACompanionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACompanionGameMode();
	virtual void RestartPlayer(AController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Companions")
	bool SetupCompanionPair(ACompanionSwitchPlayerController* PlayerController);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companions")
	TSubclassOf<ACompanionCharacterBase> StrongCompanionClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companions", meta = (ClampMin = "100.0", Units = "cm"))
	float CompanionSpawnDistance = 250.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companions")
	TObjectPtr<ACompanionCharacterBase> AgileCompanion;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companions")
	TObjectPtr<ACompanionCharacterBase> StrongCompanion;

private:
	ACompanionCharacterBase* SpawnStrongCompanion();
};

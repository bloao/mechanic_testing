#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CompanionGameMode.generated.h"

class ACompanionCharacterBase;
class ACompanionSwitchPlayerController;
class ANavMeshBoundsVolume;

UCLASS(Blueprintable)
class MECHANIC_TESTING_API ACompanionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACompanionGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Companions")
	bool SetupCompanionPair(ACompanionSwitchPlayerController* PlayerController);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companions")
	TSubclassOf<ACompanionCharacterBase> StrongCompanionClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companions", meta = (ClampMin = "100.0", Units = "cm"))
	float CompanionSpawnDistance = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Companions|Navigation", meta = (ToolTip = "Add transient navigation bounds around the level's collision geometry only when no authored bounds exist."))
	bool bCreateNavigationBoundsIfMissing = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companions")
	TObjectPtr<ACompanionCharacterBase> AgileCompanion;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companions")
	TObjectPtr<ACompanionCharacterBase> StrongCompanion;

private:
	friend class FCompanionLevelIntegrationTest;
	void PreparePlacedPlayerPawns();
	ANavMeshBoundsVolume* EnsureNavigationBounds();
	ACompanionCharacterBase* SpawnStrongCompanion();
};

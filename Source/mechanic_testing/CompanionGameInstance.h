#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CompanionGameInstance.generated.h"

/** Selects the complete companion setup for project maps without resaving them. */
UCLASS()
class MECHANIC_TESTING_API UCompanionGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual AGameModeBase* CreateGameModeForURL(FURL InURL, UWorld* InWorld) override;
	virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
		const FString& MapName, const FString& Options, const FString& Portal) const override;

private:
	bool bCreatingCompanionGameplayMode = false;
};

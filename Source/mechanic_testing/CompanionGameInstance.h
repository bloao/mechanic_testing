#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CompanionGameInstance.generated.h"

/** Selects the complete companion setup without resaving the gameplay maps. */
UCLASS()
class MECHANIC_TESTING_API UCompanionGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
		const FString& MapName, const FString& Options, const FString& Portal) const override;
};

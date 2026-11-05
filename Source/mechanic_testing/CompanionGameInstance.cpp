#include "CompanionGameInstance.h"

#include "CompanionGameMode.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

TSubclassOf<AGameModeBase> UCompanionGameInstance::OverrideGameModeClass(
	TSubclassOf<AGameModeBase> GameModeClass, const FString& MapName, const FString& Options, const FString& Portal) const
{
	const FString LevelName = UWorld::RemovePIEPrefix(FPaths::GetBaseFilename(MapName));
	if (LevelName.Equals(TEXT("Lvl_ThirdPerson"), ESearchCase::IgnoreCase)
		|| LevelName.Equals(TEXT("LVL_level2"), ESearchCase::IgnoreCase))
	{
		// A map-local template GameMode can otherwise bypass the project default.
		return ACompanionGameMode::StaticClass();
	}
	return Super::OverrideGameModeClass(GameModeClass, MapName, Options, Portal);
}

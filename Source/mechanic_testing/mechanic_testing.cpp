#include "CompanionGameMode.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

class FMechanicTestingModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		WorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddStatic(&ConfigureCompanionWorld);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializationHandle);
	}

private:
	static void ConfigureCompanionWorld(UWorld* World, const UWorld::InitializationValues)
	{
		if (!World->IsGameWorld() || !World->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
		{
			return;
		}

		// Set the runtime world's default before Unreal creates its GameMode.
		// Level doors can keep using BP_ABL_GameInstance and maps need no resave.
		AWorldSettings* Settings = World->GetWorldSettings();
		if (Settings && (!Settings->DefaultGameMode
			|| !Settings->DefaultGameMode->IsChildOf(ACompanionGameMode::StaticClass())))
		{
			Settings->DefaultGameMode = ACompanionGameMode::StaticClass();
		}
	}

	FDelegateHandle WorldInitializationHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMechanicTestingModule, mechanic_testing, "mechanic_testing");

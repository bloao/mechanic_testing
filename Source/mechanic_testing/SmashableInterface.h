#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SmashableInterface.generated.h"

UINTERFACE(BlueprintType)
class MECHANIC_TESTING_API USmashableInterface : public UInterface
{
	GENERATED_BODY()
};

class MECHANIC_TESTING_API ISmashableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Abilities|Smash")
	bool CanBeSmashedBy(AActor* SourceActor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Abilities|Smash")
	void ReceiveSmash(AActor* SourceActor);
};

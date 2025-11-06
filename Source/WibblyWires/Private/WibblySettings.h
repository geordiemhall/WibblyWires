// Copyright Beethoven & Dinosaur. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WibblySettings.generated.h"

/**
 * 
 */
UCLASS(config=EditorPerProjectUserSettings)
class WIBBLYWIRES_API UWibblySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	bool bEnableWibblyWires = true;
};

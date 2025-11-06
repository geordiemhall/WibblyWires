// Copyright 2022 Geordie Hall. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

enum EBlueprintType : int;

class FWibblyWiresModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandleBlueprintEditorOpened(EBlueprintType BlueprintType);
};

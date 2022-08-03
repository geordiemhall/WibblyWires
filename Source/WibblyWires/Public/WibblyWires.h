// Copyright 2022 Geordie Hall. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class FWibblyWiresModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

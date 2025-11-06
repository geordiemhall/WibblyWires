// Copyright 2022 Geordie Hall. All rights reserved.

#include "WibblyWires.h"

#include "BlueprintEditorModule.h"
#include "EdGraphUtilities.h"
#include "WibblyConnectionDrawingPolicy.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "BlueprintEditorModule.h"

#define LOCTEXT_NAMESPACE "FWibblyWiresModule"

static TSharedPtr<FWibblyConnectionDrawingPolicy::Factory> GraphConnectionFactory;

void FWibblyWiresModule::StartupModule()
{
	GraphConnectionFactory = MakeShared<FWibblyConnectionDrawingPolicy::Factory>();
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(GraphConnectionFactory);
}

void FWibblyWiresModule::ShutdownModule()
{
	if (GraphConnectionFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinConnectionFactory(GraphConnectionFactory);
		GraphConnectionFactory = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWibblyWiresModule, WibblyWires)
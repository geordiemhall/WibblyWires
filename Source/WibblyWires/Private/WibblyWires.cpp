// Copyright 2022 Geordie Hall. All rights reserved.

#include "WibblyWires.h"

#include "BlueprintEditorModule.h"
#include "EdGraphUtilities.h"
#include "WibblyConnectionDrawingPolicy.h"
#include "WibblySettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "BlueprintEditorModule.h"

#define LOCTEXT_NAMESPACE "FWibblyWiresModule"

static TSharedPtr<FWibblyConnectionDrawingPolicy::Factory> GraphConnectionFactory;
static int32 NumBlueprintsOpened = 0;
static TSharedPtr<SNotificationItem> DisableNotificationItem;

constexpr bool bOnlyActiveForAprilFools = true;
constexpr int32 ShowUntilAprilDateNumber = 1; // Monday is public holiday so give it another couple days until people realise and delete the plugin
constexpr int32 OfferToDisableAfterNumBlueprints = 3;
constexpr bool bPretendIsAprilFools = false;

static bool IsAprilFools()
{
	if (bPretendIsAprilFools)
	{
		return true;
	}

	// April
	if (FDateTime::Now().GetMonth() == 4)
	{
		const int32 DayNumber = FDateTime::Now().GetDay(); // From 1 - 31
		if (DayNumber >= 1 && DayNumber <= ShowUntilAprilDateNumber)
		{
			return true;
		}
	}

	return false;
}

void FWibblyWiresModule::StartupModule()
{
	// If day is not April 1st, then bail
	const bool bIsAprilFools = IsAprilFools();
	if (bOnlyActiveForAprilFools && !bIsAprilFools)
	{
		return;
	}
	
	GraphConnectionFactory = MakeShared<FWibblyConnectionDrawingPolicy::Factory>();
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(GraphConnectionFactory);

	if (bIsAprilFools)
	{
		// Listen for blueprints being opened, and show a notification after you've opened your Nth blueprint
		// that allows for turning things off
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::GetModuleChecked<FBlueprintEditorModule>("Kismet");
		BlueprintEditorModule.OnBlueprintEditorOpened().AddRaw(this, &FWibblyWiresModule::HandleBlueprintEditorOpened);
	}
}

void FWibblyWiresModule::ShutdownModule()
{
	if (GraphConnectionFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinConnectionFactory(GraphConnectionFactory);
		GraphConnectionFactory = nullptr;
	}
}

void FWibblyWiresModule::HandleBlueprintEditorOpened(EBlueprintType BlueprintType)
{
	NumBlueprintsOpened++;

	if (NumBlueprintsOpened == OfferToDisableAfterNumBlueprints)
	{
		if (!GetDefault<UWibblySettings>()->bEnableWibblyWires)
		{
			return;
		}
		
		FNotificationInfo Notification(INVTEXT("Wibbly Wires"));
		Notification.SubText = INVTEXT("Okay that's probably enough fooling around! Unless you're having fun...");
		Notification.bFireAndForget = false;
		Notification.bUseLargeFont = true;
		Notification.bUseThrobber = false;
		Notification.FadeOutDuration = 3.f;
		
		Notification.ButtonDetails.Add(FNotificationButtonInfo(
			FText::FromString(TEXT("Keep Things Wibbly!")),
			FText(),
			FSimpleDelegate::CreateLambda([]
			{
				if (DisableNotificationItem)
				{
					DisableNotificationItem->SetCompletionState(SNotificationItem::CS_Success);
					DisableNotificationItem->SetSubText(INVTEXT("What a great choice! See I knew I liked you. Stay wibbly, friend."));
					DisableNotificationItem->SetExpireDuration(3.f);
					DisableNotificationItem->ExpireAndFadeout();
					DisableNotificationItem.Reset();
				}
			}),
			SNotificationItem::CS_Pending
		));

		Notification.ButtonDetails.Add(FNotificationButtonInfo(
			FText::FromString(TEXT("No More Wibble")),
			FText(),
			FSimpleDelegate::CreateLambda([]
			{
				if (GraphConnectionFactory.IsValid())
				{
					FEdGraphUtilities::UnregisterVisualPinConnectionFactory(GraphConnectionFactory);
					GraphConnectionFactory = nullptr;
					GetMutableDefault<UWibblySettings>()->bEnableWibblyWires = false;
					GetMutableDefault<UWibblySettings>()->SaveConfig();
				}

				if (DisableNotificationItem)
				{
					DisableNotificationItem->SetCompletionState(SNotificationItem::CS_Success);
					DisableNotificationItem->SetSubText(INVTEXT("Fine. Enjoy your boring static wires."));
					DisableNotificationItem->SetExpireDuration(3.f);
					DisableNotificationItem->ExpireAndFadeout();
					DisableNotificationItem.Reset();
				}
			}),
			SNotificationItem::CS_Pending
		));
		
		DisableNotificationItem = FSlateNotificationManager::Get().AddNotification(Notification);
		if (DisableNotificationItem.IsValid())
		{
			DisableNotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWibblyWiresModule, WibblyWires)
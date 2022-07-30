// Copyright 2021 Geordie Hall. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "ConnectionDrawingPolicy.h"
#include "EdGraphUtilities.h"

/**
 * A drawing policy that wibbles
 */
class FWibblyConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:

	struct Factory : public FGraphPanelPinConnectionFactory
	{
	public:
		virtual ~Factory() = default;

		// FGraphPanelPinConnectionFactory
		virtual class FConnectionDrawingPolicy* CreateConnectionPolicy(
			const UEdGraphSchema* Schema,
			int32 InBackLayerID,
			int32 InFrontLayerID,
			float InZoomFactor,
			const FSlateRect& InClippingRect,
			FSlateWindowElementList& InDrawElements,
			UEdGraph* InGraphObj) const override;
		// ~FGraphPanelPinConnectionFactory
	};

	FWibblyConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	virtual void DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params) override;
	virtual void DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params) override;

private:

	UEdGraph* GraphObj;
};

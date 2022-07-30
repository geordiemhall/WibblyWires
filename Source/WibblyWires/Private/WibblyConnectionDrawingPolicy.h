// Copyright 2021 Geordie Hall. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "ConnectionDrawingPolicy.h"
#include "Verlet.h"
#include "EdGraphUtilities.h"
#include "Engine/SpringInterpolator.h"

struct FWireId
{
	FWireId(UEdGraphPin* InStartPin, UEdGraphPin* InEndPin)
	: StartPin(InStartPin)
	, EndPin(InEndPin)
	, StartPinHandle(FGraphPinHandle(InStartPin))
	, EndPinHandle(FGraphPinHandle(InEndPin))
	{
		uint32 StartHash = StartPin ? GetTypeHash(StartPin->PinId) : 0;
		uint32 EndHash = EndPin ? GetTypeHash(EndPin->PinId) : 0;
		Hash = HashCombine(StartHash, EndHash);
	}

	FORCEINLINE bool operator ==(const FWireId& Other) const
	{
		return StartPin == Other.StartPin && EndPin == Other.EndPin;
	}

	FORCEINLINE bool operator !=(const FWireId& Other) const
	{
		return StartPin != Other.StartPin && EndPin != Other.EndPin;
	}

	friend uint32 GetTypeHash(const FWireId& WireId)
	{
		return WireId.Hash;
	}

	bool IsPreviewConnector() const
	{
		return StartPin == nullptr || EndPin == nullptr;
	}

	const UEdGraphPin* GetConnectedPin() const
	{
		return StartPin == nullptr ? EndPin : StartPin;
	}

public:
	const UEdGraphPin* StartPin;
	const UEdGraphPin* EndPin;
	FGraphPinHandle StartPinHandle;
	FGraphPinHandle EndPinHandle;

private:
	uint32 Hash;
};

struct FWireState
{
	float DesiredRopeLength;
	float LerpedRopeLength;
	FVector2D DesiredRopeCenterPoint;
	FRK4SpringInterpolator<FVector2D> SpringCenterPoint;
	float DesiredSlackMultiplier;
	FVector2D LastStartPoint;
	FVector2D LastEndPoint;
	FLinearColor Color;

	FWireState() = default;
	FWireState(FVector2D StartPoint, FVector2D EndPoint, float SpringStiffness, float SpringDampeningRatio, float InDesiredSlackMultiplier);

	FVector2D CalculateDesiredCenterPointWithRopeLengthDelta(FVector2D StartPoint, FVector2D EndPoint, float RopeLengthDelta);
	FVector2D CalculateDesiredCenterPoint(FVector2D StartPoint, FVector2D EndPoint);
	float CalculateDesiredRopeLength(FVector2D StartPoint, FVector2D EndPoint);
	FVector2D Update(FVector2D StartPoint, FVector2D EndPoint, float DeltaTime);
};

struct FGraphState
{
	TMap<FWireId, FWireState> Wires;
	FVerletState VerletWires;
};

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

	virtual void DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params) override;

private:

	UEdGraph* GraphObj;
	FGraphState& GraphState;
};

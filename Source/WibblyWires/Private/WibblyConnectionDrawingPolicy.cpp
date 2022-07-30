// Copyright 2021 Geordie Hall. All rights reserved.


#include "WibblyConnectionDrawingPolicy.h"

#include "EdGraphSchema_K2.h"

static int32 EnableWibblyWires = 1;
FAutoConsoleVariableRef CVarEnableWibblyWires(
	TEXT("WibblyWires.Enabled"),
	EnableWibblyWires,
	TEXT("Whether BP wires should be Wibbly."));

FConnectionDrawingPolicy* FWibblyConnectionDrawingPolicy::Factory::CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	if (EnableWibblyWires && Schema->IsA(UEdGraphSchema_K2::StaticClass()))
	{
		return new FWibblyConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}

	return nullptr;
}

FWibblyConnectionDrawingPolicy::FWibblyConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	, GraphObj(InGraphObj)
{
}

void FWibblyConnectionDrawingPolicy::DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params)
{
	FConnectionParams MutParams = Params;
	MutParams.WireThickness = 20.f;
	FConnectionDrawingPolicy::DrawSplineWithArrow(StartPoint, EndPoint, MutParams);
}

void FWibblyConnectionDrawingPolicy::DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params)
{
	const FVector2D& P0 = Start;
	const FVector2D& P1 = End;

	const FVector2D SplineTangent = ComputeSplineTangent(P0, P1);
	const FVector2D P0Tangent = (Params.StartDirection == EGPD_Output) ? SplineTangent : -SplineTangent;
	const FVector2D P1Tangent = (Params.EndDirection == EGPD_Input) ? SplineTangent : -SplineTangent;

	if (Settings->bTreatSplinesLikePins)
	{
		// Distance to consider as an overlap
		const float QueryDistanceTriggerThresholdSquared = FMath::Square(Settings->SplineHoverTolerance + Params.WireThickness * 0.5f);

		// Distance to pass the bounding box cull test. This is used for the bCloseToSpline output that can be used as a
		// dead zone to avoid mistakes caused by missing a double-click on a connection.
		const float QueryDistanceForCloseSquared = FMath::Square(FMath::Sqrt(QueryDistanceTriggerThresholdSquared) + Settings->SplineCloseTolerance);

		bool bCloseToSpline = false;
		{
			// The curve will include the endpoints but can extend out of a tight bounds because of the tangents
			// P0Tangent coefficient maximizes to 4/27 at a=1/3, and P1Tangent minimizes to -4/27 at a=2/3.
			const float MaximumTangentContribution = 4.0f / 27.0f;
			FBox2D Bounds(ForceInit);

			Bounds += FVector2D(P0);
			Bounds += FVector2D(P0 + MaximumTangentContribution * P0Tangent);
			Bounds += FVector2D(P1);
			Bounds += FVector2D(P1 - MaximumTangentContribution * P1Tangent);

			bCloseToSpline = Bounds.ComputeSquaredDistanceToPoint(LocalMousePosition) < QueryDistanceForCloseSquared;

			// Draw the bounding box for debugging
#if 0
#define DrawSpaceLine(Point1, Point2, DebugWireColor) {const FVector2D FakeTangent = (Point2 - Point1).GetSafeNormal(); FSlateDrawElement::MakeDrawSpaceSpline(DrawElementsList, LayerId, Point1, FakeTangent, Point2, FakeTangent, ClippingRect, 1.0f, ESlateDrawEffect::None, DebugWireColor); }

			if (bCloseToSpline)
			{
				const FLinearColor BoundsWireColor = bCloseToSpline ? FLinearColor::Green : FLinearColor::White;

				FVector2D TL = Bounds.Min;
				FVector2D BR = Bounds.Max;
				FVector2D TR = FVector2D(Bounds.Max.X, Bounds.Min.Y);
				FVector2D BL = FVector2D(Bounds.Min.X, Bounds.Max.Y);

				DrawSpaceLine(TL, TR, BoundsWireColor);
				DrawSpaceLine(TR, BR, BoundsWireColor);
				DrawSpaceLine(BR, BL, BoundsWireColor);
				DrawSpaceLine(BL, TL, BoundsWireColor);
			}
#endif
		}

		if (bCloseToSpline)
		{
			// Find the closest approach to the spline
			FVector2D ClosestPoint(ForceInit);
			float ClosestDistanceSquared = FLT_MAX;

			const int32 NumStepsToTest = 16;
			const float StepInterval = 1.0f / (float)NumStepsToTest;
			FVector2D Point1 = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, 0.0f);
			for (float TestAlpha = 0.0f; TestAlpha < 1.0f; TestAlpha += StepInterval)
			{
				const FVector2D Point2 = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, TestAlpha + StepInterval);

				const FVector2D ClosestPointToSegment = FMath::ClosestPointOnSegment2D(LocalMousePosition, Point1, Point2);
				const float DistanceSquared = (LocalMousePosition - ClosestPointToSegment).SizeSquared();

				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestPoint = ClosestPointToSegment;
				}

				Point1 = Point2;
			}

			// Record the overlap
			if (ClosestDistanceSquared < QueryDistanceTriggerThresholdSquared)
			{
				if (ClosestDistanceSquared < SplineOverlapResult.GetDistanceSquared())
				{
					const float SquaredDistToPin1 = (Params.AssociatedPin1 != nullptr) ? (P0 - ClosestPoint).SizeSquared() : FLT_MAX;
					const float SquaredDistToPin2 = (Params.AssociatedPin2 != nullptr) ? (P1 - ClosestPoint).SizeSquared() : FLT_MAX;

					SplineOverlapResult = FGraphSplineOverlapResult(Params.AssociatedPin1, Params.AssociatedPin2, ClosestDistanceSquared, SquaredDistToPin1, SquaredDistToPin2, true);
				}
			}
			else if (ClosestDistanceSquared < QueryDistanceForCloseSquared)
			{
				SplineOverlapResult.SetCloseToSpline(true);
			}
		}
	}

	// Draw the spline itself
	FSlateDrawElement::MakeDrawSpaceSpline(
		DrawElementsList,
		LayerId,
		P0, P0Tangent,
		P1, P1Tangent,
		Params.WireThickness,
		ESlateDrawEffect::None,
		Params.WireColor
	);

	if (Params.bDrawBubbles || (MidpointImage != nullptr))
	{
		// This table maps distance along curve to alpha
		FInterpCurve<float> SplineReparamTable;
		const float SplineLength = MakeSplineReparamTable(P0, P0Tangent, P1, P1Tangent, SplineReparamTable);

		// Draw bubbles on the spline
		if (Params.bDrawBubbles)
		{
			const float BubbleSpacing = 64.f * ZoomFactor;
			const float BubbleSpeed = 192.f * ZoomFactor;
			const FVector2D BubbleSize = BubbleImage->ImageSize * ZoomFactor * 0.2f * Params.WireThickness;

			float Time = (FPlatformTime::Seconds() - GStartTime);
			const float BubbleOffset = FMath::Fmod(Time * BubbleSpeed, BubbleSpacing);
			const int32 NumBubbles = FMath::CeilToInt(SplineLength/BubbleSpacing);
			for (int32 i = 0; i < NumBubbles; ++i)
			{
				const float Distance = ((float)i * BubbleSpacing) + BubbleOffset;
				if (Distance < SplineLength)
				{
					const float Alpha = SplineReparamTable.Eval(Distance, 0.f);
					FVector2D BubblePos = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, Alpha);
					BubblePos -= (BubbleSize * 0.5f);

					FSlateDrawElement::MakeBox(
						DrawElementsList,
						LayerId,
						FPaintGeometry( BubblePos, BubbleSize, ZoomFactor  ),
						BubbleImage,
						ESlateDrawEffect::None,
						Params.WireColor
						);
				}
			}
		}

		// Draw the midpoint image
		if (MidpointImage != nullptr)
		{
			// Determine the spline position for the midpoint
			const float MidpointAlpha = SplineReparamTable.Eval(SplineLength * 0.5f, 0.f);
			const FVector2D Midpoint = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha);

			// Approximate the slope at the midpoint (to orient the midpoint image to the spline)
			const FVector2D MidpointPlusE = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha + KINDA_SMALL_NUMBER);
			const FVector2D MidpointMinusE = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha - KINDA_SMALL_NUMBER);
			const FVector2D SlopeUnnormalized = MidpointPlusE - MidpointMinusE;

			// Draw the arrow
			const FVector2D MidpointDrawPos = Midpoint - MidpointRadius;
			const float AngleInRadians = SlopeUnnormalized.IsNearlyZero() ? 0.0f : FMath::Atan2(SlopeUnnormalized.Y, SlopeUnnormalized.X);

			FSlateDrawElement::MakeRotatedBox(
				DrawElementsList,
				LayerId,
				FPaintGeometry(MidpointDrawPos, MidpointImage->ImageSize * ZoomFactor, ZoomFactor),
				MidpointImage,
				ESlateDrawEffect::None,
				AngleInRadians,
				TOptional<FVector2D>(),
				FSlateDrawElement::RelativeToElement,
				Params.WireColor
				);
		}
	}

}
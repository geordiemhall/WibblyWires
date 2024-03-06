// Copyright 2022 Geordie Hall. All rights reserved.


#include "WibblyConnectionDrawingPolicy.h"

#include "EdGraphSchema_K2.h"
#include "Verlet.h"
#include "Engine/SpringInterpolator.h"

// For each graph guid, store a map from wire id to wire state
static TMap<FGuid, FGraphState> GraphStates;

static int32 EnableWibblyWires = 1;
FAutoConsoleVariableRef CVarEnableWibblyWires(
	TEXT("WibblyWires.Enabled"),
	EnableWibblyWires,
	TEXT("Whether BP wires should be Wibbly."));

static float ThicknessMultiplier = 1.5f;
FAutoConsoleVariableRef CVarThicknessMultiplier(
	TEXT("WibblyWires.ThicknessMultiplier"),
	ThicknessMultiplier,
	TEXT("How much thicker to draw the wire lines."));

static int32 BounceWires = 0;
FAutoConsoleVariableRef CVarBounceWires(
	TEXT("WibblyWires.BounceWires"),
	BounceWires,
	TEXT("Whether wires have some bounce when they extend too far")
);

static float RopeLengthHangMultiplier = 1.f;
FAutoConsoleVariableRef CVarWireLength(
	TEXT("WibblyWires.WireLength"),
	RopeLengthHangMultiplier,
	TEXT("How much extra length should wires have")
);

float WireShrinkRate = 150.f;
FAutoConsoleVariableRef CVarWireShrinkRate(
	TEXT("WibblyWires.WireShrinkRate"),
	WireShrinkRate,
	TEXT("How quickly should wires get sucked back into their nodes after having been cut")
);

float SecondsBeforeBreaking = 1.f;
FAutoConsoleVariableRef CVarSecondsBeforeBreaking(
	TEXT("WibblyWires.SecondsBeforeBreaking"),
	SecondsBeforeBreaking,
	TEXT("How many seconds should cut wires dangle before detaching from their nodes and falling")
);

float WireFriction = 0.9996f;
FAutoConsoleVariableRef CVarWireFriction(
	TEXT("WibblyWires.WireFriction"),
	WireFriction,
	TEXT("Friction multiplier for velocities, should be very close to 1.")
);

FAutoConsoleCommand CVarResetWireStates(
	TEXT("WibblyWires.ResetWireStates"),
	TEXT("Resets wire states so that they're reinitialized with latest defaults etc."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		GraphStates.Reset();
	})
);

FWireState::FWireState(FVector2D StartPoint, FVector2D EndPoint, float SpringStiffness, float SpringDampeningRatio, float InDesiredSlackMultiplier)
{
	LastStartPoint = StartPoint;
	LastEndPoint = EndPoint;
	DesiredSlackMultiplier = InDesiredSlackMultiplier;

	// Snap to the desired rope length
	DesiredRopeLength = CalculateDesiredRopeLength(StartPoint, EndPoint);
	LerpedRopeLength = DesiredRopeLength * 1.1f; // Start off a little off from desired so there's an initial bounce

	// Snap to the desired center point
	float LengthDelta = LerpedRopeLength - (EndPoint - StartPoint).Size();
	DesiredRopeCenterPoint = CalculateDesiredCenterPointWithRopeLengthDelta(StartPoint, EndPoint, LengthDelta);
	SpringCenterPoint.SetSpringConstants(SpringStiffness, SpringDampeningRatio);
	SpringCenterPoint.Reset(FVector(DesiredRopeCenterPoint, 0.f));
}

FVector2D FWireState::CalculateDesiredCenterPointWithRopeLengthDelta(FVector2D StartPoint, FVector2D EndPoint, float RopeLengthDelta)
{
	FVector2D Center = CalculateDesiredCenterPoint(StartPoint, EndPoint);
	Center.Y += RopeLengthDelta * RopeLengthHangMultiplier;
	return Center;
}

FVector2D FWireState::CalculateDesiredCenterPoint(FVector2D StartPoint, FVector2D EndPoint)
{
	if (StartPoint.X > EndPoint.X)
	{
		Swap(StartPoint, EndPoint);
	}

	FVector2D Delta = EndPoint - StartPoint;
	FVector2D Direction = Delta.GetSafeNormal();
	FVector2D UpDirection(0.f, 1.f);
	float DotWithUp = Direction | UpDirection;
	DotWithUp = FMath::Pow(FMath::Abs(DotWithUp), 2.f) * FMath::Sign(DotWithUp);
	float NormalizedDotWithUp = DotWithUp * 0.5f + 0.5f;
	float CenterX = FMath::Lerp(StartPoint.X, EndPoint.X, NormalizedDotWithUp);
	// This won't be quite the same as deriving a CenterY from the real CenterX, but we'll see how it looks cause avoids some trig
	FVector2D Center = FMath::Lerp(StartPoint, EndPoint, NormalizedDotWithUp);
	return Center;
}

float FWireState::CalculateDesiredRopeLength(FVector2D StartPoint, FVector2D EndPoint)
{
	FVector2D Delta = EndPoint - StartPoint;
	float TightRopeLength = Delta.Size();
	return TightRopeLength * DesiredSlackMultiplier;
}

FVector2D FWireState::Update(FVector2D StartPoint, FVector2D EndPoint, float DeltaTime)
{
	LastStartPoint = StartPoint;
	LastEndPoint = EndPoint;

	// Ensure start point is always the left-most point so we can make some assumptions with our math
	if (StartPoint.X > EndPoint.X)
	{
		Swap(StartPoint, EndPoint);
	}

	FVector2D Delta = EndPoint - StartPoint;

	float TightRopeLength = Delta.Size();

	DesiredRopeLength = TightRopeLength * DesiredSlackMultiplier;
	LerpedRopeLength = FMath::Max(TightRopeLength, FMath::Lerp(LerpedRopeLength, DesiredRopeLength, DeltaTime * 20.f));
	float LengthDelta = LerpedRopeLength - TightRopeLength;

	DesiredRopeCenterPoint = CalculateDesiredCenterPointWithRopeLengthDelta(StartPoint, EndPoint, LengthDelta);

	// Calculate desired center point
	FVector2D LerpedCenterPoint = FVector2D(SpringCenterPoint.Update(FVector(DesiredRopeCenterPoint, 0.f), DeltaTime));

	FVector2D Velocity = FVector2D(SpringCenterPoint.GetVelocity());
	if (BounceWires && LerpedCenterPoint.Y > DesiredRopeCenterPoint.Y && Velocity.Y > 0.1f)
	{
		Velocity.Y = FMath::Abs(Velocity.Y) * -0.9f;
		SpringCenterPoint.SetVelocity(FVector(Velocity, 0.f));
	}

	return LerpedCenterPoint;
}

FConnectionDrawingPolicy* FWibblyConnectionDrawingPolicy::Factory::CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	if (EnableWibblyWires)
	{
		if (Schema->IsA(UEdGraphSchema_K2::StaticClass()))
		{
			return new FWibblyConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
		}
	}
	else
	{
		// Release our memory if not even enabled
		GraphStates.Empty();
	}

	return nullptr;
}

namespace FRK4SpringInterpolatorUtils
{
	static FORCEINLINE bool IsValidValue(FVector2D Value, float MaxAbsoluteValue = RK4_SPRING_INTERPOLATOR_MAX_VALUE)
	{
		return Value.GetAbsMax() < MaxAbsoluteValue;
	}

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	static FORCEINLINE bool AreEqual(FVector2D A, FVector2D B, float ErrorTolerance = UE_KINDA_SMALL_NUMBER)
	{
		return A.Equals(B, ErrorTolerance);
	}
#else
	static FORCEINLINE bool AreEqual(FVector2D A, FVector2D B, float ErrorTolerance = KINDA_SMALL_NUMBER)
	{
		return A.Equals(B, ErrorTolerance);
	}
#endif

}

FWibblyConnectionDrawingPolicy::FWibblyConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	, GraphObj(InGraphObj)
	, GraphState(GraphStates.FindOrAdd(InGraphObj->GraphGuid))
{
}

void FWibblyConnectionDrawingPolicy::DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params)
{
	const FVector2D& P0 = Start;
	const FVector2D& P1 = End;

	const float DefaultStiffness = 100.f;
    const float DefaultDampeningRatio = 0.4f;

	float WireThickness = Params.WireThickness * ThicknessMultiplier * FSlateApplication::Get().GetApplicationScale() * ZoomFactor;

    const FWireId WireId(Params.AssociatedPin1, Params.AssociatedPin2);
    FWireState* WireState = GraphState.Wires.Find(WireId);

	// Create a new wire if needed
    if (!WireState)
    {
    	bool bIsPreviewConnector = Params.AssociatedPin1 == nullptr || Params.AssociatedPin2 == nullptr;
    	float StiffnessVariance = FMath::FRandRange(0.3f, 1.5f);
    	float DampeningVariance = FMath::FRandRange(0.7f, 1.2f);
    	float SlackMultiplier = 1.3f + FMath::FRandRange(0.f, 0.3f);
    	float Stiffness = DefaultStiffness * StiffnessVariance + (bIsPreviewConnector ? 0.3f : 0.f);
    	float DampeningRatio = FMath::Clamp(DefaultDampeningRatio * DampeningVariance, 0.3f, 0.9f);
    	FWireState NewWireState(Start, End, Stiffness, DampeningRatio, SlackMultiplier);
    	NewWireState.Color = Params.WireColor;

    	for (const auto& ExistingState : GraphState.Wires)
    	{
    		if (!ExistingState.Key.IsPreviewConnector())
    		{
    			continue;
    		}

    		const UEdGraphPin* ConnectedPin = ExistingState.Key.GetConnectedPin();
    		if (ConnectedPin != Params.AssociatedPin1 && ConnectedPin != Params.AssociatedPin2)
    		{
    			continue;
    		}

    		const float DistThresholdSqr = 30.f * 30.f;
			if (FVector2D::DistSquared(ExistingState.Value.LastStartPoint, Start) < DistThresholdSqr && FVector2D::DistSquared(ExistingState.Value.LastEndPoint, End) < DistThresholdSqr)
			{
				// Inherit our initial state from this existing thing, since it was probably a preview connector that got connected
				NewWireState = ExistingState.Value;
			}
    	}

    	WireState = &GraphState.Wires.Add(WireId, MoveTemp(NewWireState));
    }

	// Clamp our tick rate to 30fps to avoid editor hitches hiding our animations, we'd rather they just pause
	static const float MaxDeltaTime = 1.f / 30.f;
	const float DeltaTime = FMath::Min(FSlateApplication::Get().GetDeltaTime(), MaxDeltaTime);
    FVector2D CenterPoint = WireState->Update(Start, End, DeltaTime);

	// Don't need these anymore!
	// const FVector2D SplineTangent = ComputeSplineTangent(P0, P1);
	// const FVector2D P0Tangent = (Params.StartDirection == EGPD_Output) ? SplineTangent : -SplineTangent;
	// const FVector2D P1Tangent = (Params.EndDirection == EGPD_Input) ? SplineTangent : -SplineTangent;

	// Magic number to get more of a bend
	const FVector2D P0Tangent = (CenterPoint - P0) * 1.3f;
	const FVector2D P1Tangent = (P1 - CenterPoint) * 1.3f;

	if (Settings->bTreatSplinesLikePins)
	{
		// Distance to consider as an overlap
		const float QueryDistanceTriggerThresholdSquared = FMath::Square(Settings->SplineHoverTolerance + WireThickness * 0.5f);

		// Distance to pass the bounding box cull test. This is used for the bCloseToSpline output that can be used as a
		// dead zone to avoid mistakes caused by missing a double-click on a connection.
		const float QueryDistanceForCloseSquared = FMath::Square(FMath::Sqrt(QueryDistanceTriggerThresholdSquared) + Settings->SplineCloseTolerance);

		bool bCloseToSpline = false;
		{
			// The curve will include the endpoints but can extend out of a tight bounds because of the tangents
			// P0Tangent coefficient maximizes to 4/27 at a=1/3, and P1Tangent minimizes to -4/27 at a=2/3.
			// const float MaximumTangentContribution = 4.0f / 27.0f;

			// Note (Geordie): If we don't use the engine's tangent limits then need to use full control-point bounds
			const float MaximumTangentContribution = 1.f / 3.f;
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
		WireThickness,
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
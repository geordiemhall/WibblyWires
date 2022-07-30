#pragma once

#include "WibblyWires.h"

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
typedef FVector2f FVectorType;
typedef FBox2f FBoxType;
#else
typedef FVector2D FVectorType;
typedef FBox2D FBoxType;
#endif

extern float WireFriction;
extern float SecondsBeforeBreaking;
extern float WireShrinkRate;

struct FVerletPoint
{
	FVectorType Position;
	FVectorType LastPosition;
	FVectorType Acceleration;
	bool bIsPinned = false;

	FVerletPoint(FVectorType InitialPosition, bool IsPinned = false, FVectorType InitialVelocity = FVectorType::ZeroVector)
	{
		Position = InitialPosition;
		LastPosition = Position - InitialVelocity;
		bIsPinned = IsPinned;
		Acceleration = FVectorType::ZeroVector;
	}

	FVectorType CalculateVelocity() const
	{
		return Position - LastPosition;
	}

	void UpdatePosition(float deltaTime)
	{
		if (!bIsPinned)
		{
			const FVectorType Velocity = CalculateVelocity() * WireFriction;
			LastPosition = Position;
			Position = Position + Velocity + Acceleration * deltaTime * deltaTime;
		}

		Acceleration = FVectorType::ZeroVector;
	}

	void Accelerate(FVectorType Impulse)
	{
		Acceleration += Impulse;
	}

	void AddVelocity(FVectorType Velocity)
	{
		LastPosition -= Velocity;
	}
};

struct FVerletStick
{
	int32 Point0Index;
	int32 Point1Index;
	float DesiredLength;

	FVerletStick() = delete;

	FVerletStick(int32 InPoint0, int32 InPoint1, float InDesiredLength)
		: Point0Index(InPoint0)
		, Point1Index(InPoint1)
		, DesiredLength(InDesiredLength)
	{
	}

	void ConstrainLength(FVerletPoint& Point0, FVerletPoint& Point1)
	{
		FVectorType Delta = Point1.Position - Point0.Position;
		float CurrentLength = Delta.Size();
		float Difference = DesiredLength - CurrentLength;
		float HalfPercent = (Difference / CurrentLength) * 0.5f;
		FVectorType HalfOffset = Delta * HalfPercent;

		// If either is pinned, the the other will need to cover the full adjustment
		// If both are pinned then this will be wasted, but not micro-optimizing yet
		if (Point0.bIsPinned || Point1.bIsPinned)
		{
			HalfOffset *= 2.f;
		}

		if (!Point0.bIsPinned)
		{
			Point0.Position -= HalfOffset;
		}

		if (!Point1.bIsPinned)
		{
			Point1.Position += HalfOffset;
		}
	}
};

struct FVerletChain
{
	FVectorType Gravity = FVectorType(0.f, 1500.f);
	TArray<FVerletPoint> Points;
	TArray<FVerletStick> Sticks;
	FLinearColor LineColor;
	float LineThickness;
	bool bHasFullyShrunk = false;
	float CreationTime;
	bool bHasBroken = false;

	FVerletChain(FLinearColor InLineColor, float InLineThickness)
	{
		LineColor = InLineColor;
		LineThickness = InLineThickness;
		CreationTime = FSlateApplication::Get().GetCurrentTime();
	}

	// Adds a new point and automatically connects it to the previous point with a stick
	void AddToChain(FVectorType NewPoint, bool bIsPinned = false)
	{
		Points.Add(FVerletPoint(NewPoint, bIsPinned));

		int32 PointCount = Points.Num();
		if (PointCount >= 2)
		{
			int32 P0Index = PointCount - 2;
			int32 P1Index = PointCount - 1;
			FVerletPoint P0 = Points[P0Index];
			FVerletPoint P1 = Points[P1Index];
			float DesiredLength = FVectorType::Distance(P0.Position, P1.Position);
			Sticks.Add(FVerletStick(P0Index, P1Index, DesiredLength));
		}
	}

	// Offsets the whole simulation by some translation
	void Translate(FVectorType Translation)
	{
		for (FVerletPoint& Point : Points)
		{
			Point.Position += Translation;
			Point.LastPosition += Translation;
		}
	}

	void ShrinkSticksBy(float Multiplier)
	{
		for (FVerletStick& Stick : Sticks)
		{
			Stick.DesiredLength *= Multiplier;
		}
	}

	void UnpinAll()
	{
		SetAllPinned(false);
	}

	void SetAllPinned(bool bIsPinned)
	{
		for (FVerletPoint& Point : Points)
		{
			Point.bIsPinned = bIsPinned;
		}
	}

	float GetSecondsSinceCreated() const
	{
		return FSlateApplication::Get().GetCurrentTime() - CreationTime;
	}

	void Update(float DeltaTime)
	{
		static const float MaxDeltaTime = 1.0f / 30.f;
		DeltaTime = FMath::Min(MaxDeltaTime, DeltaTime);

		float TimeSinceCreated = GetSecondsSinceCreated();
		if (TimeSinceCreated > SecondsBeforeBreaking && !bHasBroken)
		{
			SetAllPinned(false);
			bHasBroken = true;
		}

		const int32 Substeps = 10;
		float SubDeltaTime = DeltaTime / Substeps;
		for (int32 i = 0; i < Substeps; i++)
		{
			ApplyGravity();

			UpdatePositions(SubDeltaTime);

			for (int32 j = 0; j < 5; j++)
			{
				ApplyConstraints();
				ApplyCollisions();
			}
		}
	}

	FBoxType CalcBounds() const
	{
		FBoxType Bounds(EForceInit::ForceInitToZero);

		for (const FVerletPoint& Point : Points)
		{
			Bounds += Point.Position;
		}

		return Bounds;
	}

private:

	void ShrinkSticks(float DeltaTime)
	{
		bHasFullyShrunk = true;

		for (int32 i = 0; i < Sticks.Num(); i++)
		{
			FVerletStick& Stick = Sticks[i];

			if (Stick.DesiredLength < 1.f)
			{
				Stick.DesiredLength = 0.1f;
				Points[Stick.Point0Index].bIsPinned = true;
				Points[Stick.Point1Index].bIsPinned = true;
				Points[Stick.Point1Index].Position = Points[Stick.Point0Index].Position;
			}
			else
			{
				Stick.DesiredLength = FMath::Max(Stick.DesiredLength - (WireShrinkRate * DeltaTime), 0.1f);
				bHasFullyShrunk = false;
				break;
			}
		}
	}

	void UpdatePositions(float DeltaTime)
	{
		for (FVerletPoint& Point : Points)
		{
			Point.UpdatePosition(DeltaTime);
		}
	}

	void ApplyConstraints()
	{
		for (FVerletStick& Stick : Sticks)
		{
			Stick.ConstrainLength(Points[Stick.Point0Index], Points[Stick.Point1Index]);
		}
	}

	void ApplyCollisions()
	{
		// Nothing for now
	}

	void ApplyGravity()
	{
		for (FVerletPoint& Point : Points)
		{
			Point.Accelerate(Gravity);
		}
	}
};

class FVerletState
{
public:
	void TranslateVerletChains(FVectorType Translation)
	{
		for (FVerletChain& Chain : VerletChains)
		{
			Chain.Translate(Translation);
		}
	}

	void UpdateVerletChains(float DeltaTime)
	{
		for (FVerletChain& Chain : VerletChains)
		{
			Chain.Update(DeltaTime);
		}

		// Delete any chains that are entirely below the bottom of the screen
		VerletChains.RemoveAllSwap([](const FVerletChain& Chain)
		{
			if (Chain.GetSecondsSinceCreated() > 30.f)
			{
				return true;
			}

			FBoxType Bounds = Chain.CalcBounds();
			if (Bounds.Min.Y > 2000.f || Bounds.Max.Y < -1000.f || Bounds.Min.X > 3000.f || Bounds.Max.X < -1000.f)
			{
				return true;
			}

			return false;
		});
	}

	void RenderVerletChains(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, float ThicknessScale)
	{
		int32 MaxPointCount = 0;

		for (const FVerletChain& Chain : VerletChains)
		{
			MaxPointCount = FMath::Max(MaxPointCount, Chain.Points.Num());
		}

		// Re-use this array between graphs and frames to save on allocations
		static TArray<FVectorType> Points;
		Points.Reserve(MaxPointCount);

		for (const FVerletChain& Chain : VerletChains)
		{
			Points.Reset();

			for (const FVerletPoint& Point : Chain.Points)
			{
				Points.Add(Point.Position);
			}

			float Opacity = FMath::Clamp(1.f - ((Chain.GetSecondsSinceCreated() - 1.f) / 1.f), 0.f, 1.f);

			// TODO: Catmull-Rom spline through these points so can get away with fewer segments
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				FPaintGeometry(),
				Points,
				ESlateDrawEffect::NoPixelSnapping,
				Chain.LineColor.CopyWithNewOpacity(Opacity),
				true, // bAntiAlias
				Chain.LineThickness * ThicknessScale);
		}
	}

private:
	TArray<FVerletChain> VerletChains;
};
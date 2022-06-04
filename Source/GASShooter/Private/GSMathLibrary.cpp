// Copyright 2020 Nuraga Wiswakarma.

#include "GSMathLibrary.h"

bool UGSMathLibrary::IsObservedObjectMovingRight_HeadingOnly(
    FVector ObserverLocation,
    FVector ObserverDirection,
    FVector ObservedLocation,
    FVector ObservedVelocity
    )
{
    // Observed object not moving, return false
    if (ObservedVelocity.SizeSquared2D() < KINDA_SMALL_NUMBER)
    {
        return false;
    }

    FVector2D ObserverToTargetDelta(ObservedLocation-ObserverLocation);

    if (ObserverToTargetDelta.SizeSquared() > 0.f)
    {
        FVector2D ObserverToTargetPerpRight(-ObserverToTargetDelta.Y, ObserverToTargetDelta.X);
        return (ObserverToTargetPerpRight | FVector2D(ObservedVelocity)) > 0.f;
    }
    else
    {
        FVector2D ObserverDirectionPerpRight(-ObserverDirection.Y, ObserverDirection.X);
        return (ObserverDirectionPerpRight | FVector2D(ObservedVelocity)) > 0.f;
    }
}

bool UGSMathLibrary::LineTraceSweepTowardsObservedObjectVelocity_HeadingOnly(
    const UObject* WorldContextObject, 
    FVector ObserverLocation,
    FVector ObserverDirection,
    FVector ObservedLocation,
    FVector ObservedVelocity,
    int32 Iteration,
    float SweepAngle,
    bool bStartFromObserved,
    ETraceTypeQuery TraceChannel,
    bool bTraceComplex,
    const TArray<AActor*>& ActorsToIgnore,
    EDrawDebugTrace::Type DrawDebugType,
    FHitResult& OutHit,
    bool bIgnoreSelf,
    FLinearColor TraceColor,
    FLinearColor TraceHitColor,
    float DrawTime
    )
{
    FVector2D ObserverToTargetDelta(ObservedLocation-ObserverLocation);

    // Intersecting locations, abort
    if (ObserverToTargetDelta.SizeSquared() < KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const bool bSweepRequired = ObservedVelocity.SizeSquared2D() > 0.f && Iteration > 1 && FMath::Abs(SweepAngle) > 0.f;

    FVector2D ObserverToTargetDir;
    float ObserverToTargetLength;
    ObserverToTargetDelta.ToDirectionAndLength(ObserverToTargetDir, ObserverToTargetLength);

    const float IterationAngle = IsObservedObjectMovingRight_HeadingOnly(
            ObserverLocation,
            ObserverDirection,
            ObservedLocation,
            ObservedVelocity
            )
        ? SweepAngle
        : -SweepAngle;

    if (bSweepRequired)
    {
        for (int32 i=0; i<Iteration; ++i)
        {
            const FVector TraceDir(ObserverToTargetDir.GetRotated(i*IterationAngle), 0.f);
            FVector TraceStart;
            FVector TraceEnd;

            if (bStartFromObserved)
            {
                TraceStart = ObserverLocation + TraceDir * ObserverToTargetLength;
                TraceEnd = ObserverLocation;
            }
            else
            {
                TraceStart = ObserverLocation;
                TraceEnd = ObserverLocation + TraceDir * ObserverToTargetLength;
            }

            FHitResult TempHit;
            bool bTraceHit = UKismetSystemLibrary::LineTraceSingle(
                WorldContextObject,
                TraceStart,
                TraceEnd,
                TraceChannel,
                bTraceComplex,
                ActorsToIgnore,
                DrawDebugType,
                TempHit,
                bIgnoreSelf,
                TraceColor,
                TraceHitColor,
                DrawTime
                );

            if (bTraceHit)
            {
                OutHit = MoveTemp(TempHit);
                return true;
            }
        }
    }
    else
    {
        FVector TraceStart;
        FVector TraceEnd;

        if (bStartFromObserved)
        {
            TraceStart = ObservedLocation;
            TraceEnd = ObserverLocation;
        }
        else
        {
            TraceStart = ObserverLocation;
            TraceEnd = ObservedLocation;
        }

        FHitResult TempHit;
        bool bTraceHit = UKismetSystemLibrary::LineTraceSingle(
            WorldContextObject,
            TraceStart,
            TraceEnd,
            TraceChannel,
            bTraceComplex,
            ActorsToIgnore,
            DrawDebugType,
            TempHit,
            bIgnoreSelf,
            TraceColor,
            TraceHitColor,
            DrawTime
            );

        if (bTraceHit)
        {
            OutHit = MoveTemp(TempHit);
            return true;
        }
    }

    return false;
}

void UGSMathLibrary::EvaluateBezier(
    const FVector& ControlPoint0,
    const FVector& ControlPoint1,
    const FVector& ControlPoint2,
    const FVector& ControlPoint3,
    int32 NumPoints,
    TArray<FVector>& OutPoints
    )
{
    if (NumPoints >= 2)
    {
        FVector ControlPoints[4] = {
            ControlPoint0,
            ControlPoint1,
            ControlPoint2,
            ControlPoint3
            };
        FVector::EvaluateBezier(ControlPoints, NumPoints, OutPoints);
    }
}

void UGSMathLibrary::EvaluateBezier3(
    const FVector& ControlPointA,
    const FVector& ControlPointB,
    const FVector& ControlPointC,
    float BiasAToB,
    float BiasBToC,
    int32 NumPoints,
    TArray<FVector>& OutPoints
    )
{
    if (NumPoints >= 2)
    {
        float ClampedBiasAB = FMath::Clamp(BiasAToB, 0.f, 1.f);
        float ClampedBiasBC = FMath::Clamp(BiasBToC, 0.f, 1.f);
        FVector ControlPoints[4] = {
            ControlPointA,
            ControlPointA+(ControlPointB-ControlPointA)*ClampedBiasAB,
            ControlPointB+(ControlPointC-ControlPointB)*ClampedBiasBC,
            ControlPointC
            };
        FVector::EvaluateBezier(ControlPoints, NumPoints, OutPoints);
    }
}

void UGSMathLibrary::ClosestPointsBetweenSegments(
    const FVector& SegmentAStart,
    const FVector& SegmentAEnd,
    const FVector& SegmentBStart,
    const FVector& SegmentBEnd,
    FVector& OutPointA,
    FVector& OutPointB
    )
{
    FMath::SegmentDistToSegmentSafe(
        SegmentAStart,
        SegmentBStart,
        SegmentAEnd,
        SegmentBEnd,
        OutPointA,
        OutPointB
        );
}

bool UGSMathLibrary::CapsuleAndSweepSphereIntersection(
    const FVector& CapsuleLocation,
    float CapsuleRadius,
    float CapsuleHalfHeight,
    const FVector& SphereStart,
    const FVector& SphereEnd,
    float SphereRadius,
    FVector& OutPointA,
    FVector& OutPointB
    )
{
    FVector CapsuleSegment0 = CapsuleLocation + FVector(0,0,-CapsuleHalfHeight);
    FVector CapsuleSegment1 = CapsuleLocation + FVector(0,0, CapsuleHalfHeight);

    FMath::SegmentDistToSegmentSafe(
        CapsuleSegment0,
        SphereStart,
        CapsuleSegment1,
        SphereEnd,
        OutPointA,
        OutPointB
        );

    float PointsDistSq = (OutPointB-OutPointA).SizeSquared();

    return PointsDistSq < FMath::Square(CapsuleRadius+SphereRadius);
}

bool UGSMathLibrary::SweepCapsuleAndSphereIntersection(
    const FVector& CapsuleStart,
    const FVector& CapsuleEnd,
    float CapsuleRadius,
    float CapsuleHalfHeight,
    const FVector& SphereStart,
    const FVector& SphereEnd,
    float SphereRadius,
    FVector& OutPointA,
    FVector& OutPointB
    )
{
    FMath::SegmentDistToSegmentSafe(
        CapsuleStart,
        SphereStart,
        CapsuleEnd,
        SphereEnd,
        OutPointA,
        OutPointB
        );

    FVector PointDelta = OutPointB-OutPointA;
    float RadiusSq = FMath::Square(CapsuleRadius+SphereRadius);
    float Height = CapsuleHalfHeight+CapsuleRadius+SphereRadius;

    return PointDelta.SizeSquared2D() <= RadiusSq && PointDelta.Z <= Height;
}

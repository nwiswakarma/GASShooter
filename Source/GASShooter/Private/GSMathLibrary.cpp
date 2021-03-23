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

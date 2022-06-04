// Copyright 2020 Nuraga Wiswakarma.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GSMathLibrary.generated.h"


/**
 * 
 */
UCLASS()
class GASSHOOTER_API UGSMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable)
    static bool IsObservedObjectMovingRight_HeadingOnly(
        FVector ObserverLocation,
        FVector ObserverDirection,
        FVector ObservedLocation,
        FVector ObservedVelocity
        );

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", Keywords="raycast"))
    static bool LineTraceSweepTowardsObservedObjectVelocity_HeadingOnly(
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
        FLinearColor TraceColor = FLinearColor::Red,
        FLinearColor TraceHitColor = FLinearColor::Green,
        float DrawTime = 5.0f
        );

	UFUNCTION(BlueprintCallable)
    static void EvaluateBezier(
        const FVector& ControlPoint0,
        const FVector& ControlPoint1,
        const FVector& ControlPoint2,
        const FVector& ControlPoint3,
        int32 NumPoints,
        TArray<FVector>& OutPoints
        );

	UFUNCTION(BlueprintCallable)
    static void EvaluateBezier3(
        const FVector& ControlPointA,
        const FVector& ControlPointB,
        const FVector& ControlPointC,
        float BiasAToB,
        float BiasBToC,
        int32 NumPoints,
        TArray<FVector>& OutPoints
        );

	UFUNCTION(BlueprintCallable)
    static void ClosestPointsBetweenSegments(
        const FVector& SegmentAStart,
        const FVector& SegmentAEnd,
        const FVector& SegmentBStart,
        const FVector& SegmentBEnd,
        FVector& OutPointA,
        FVector& OutPointB
        );

	UFUNCTION(BlueprintCallable)
    static bool CapsuleAndSweepSphereIntersection(
        const FVector& CapsuleLocation,
        float CapsuleRadius,
        float CapsuleHalfHeight,
        const FVector& SphereStart,
        const FVector& SphereEnd,
        float SphereRadius,
        FVector& OutPointA,
        FVector& OutPointB
        );

	UFUNCTION(BlueprintCallable)
    static bool SweepCapsuleAndSphereIntersection(
        const FVector& CapsuleStart,
        const FVector& CapsuleEnd,
        float CapsuleRadius,
        float CapsuleHalfHeight,
        const FVector& SphereStart,
        const FVector& SphereEnd,
        float SphereRadius,
        FVector& OutPointA,
        FVector& OutPointB
        );
};

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

};

// Copyright 2021 Nuraga Wiswakarma.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GSHUD.generated.h"

/**
 * 
 */
UCLASS()
class GASSHOOTER_API AGSHUD : public AHUD
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable)
	void ProjectBoundsByExtents(
        const FVector& Origin,
        const FVector& Extents,
        FVector2D& OutScreenTL,
        FVector2D& OutScreenTR,
        FVector2D& OutScreenBR,
        FVector2D& OutScreenBL
        );

	UFUNCTION(BlueprintCallable)
	void ProjectBoundsBySphereRadius(
        const FVector& Origin,
        float SphereRadius,
        FVector2D& OutScreenTL,
        FVector2D& OutScreenTR,
        FVector2D& OutScreenBR,
        FVector2D& OutScreenBL
        );
};

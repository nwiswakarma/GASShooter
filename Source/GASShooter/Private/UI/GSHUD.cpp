// Copyright 2021 Nuraga Wiswakarma.


#include "UI/GSHUD.h"
#include "PaperSprite.h"

void AGSHUD::ProjectBoundsByExtents(
    const FVector& Origin,
    const FVector& Extents,
    FVector2D& OutScreenTL,
    FVector2D& OutScreenTR,
    FVector2D& OutScreenBR,
    FVector2D& OutScreenBL
    )
{
    FBox2D Bounds2D(ForceInitToZero);

    Bounds2D += FVector2D(Project(Origin+FVector(-Extents.X, -Extents.Y, -Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector( Extents.X, -Extents.Y, -Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector(-Extents.X,  Extents.Y, -Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector(-Extents.X, -Extents.Y,  Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector(-Extents.X,  Extents.Y,  Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector( Extents.X, -Extents.Y,  Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector( Extents.X,  Extents.Y, -Extents.Z)));
    Bounds2D += FVector2D(Project(Origin+FVector( Extents.X,  Extents.Y,  Extents.Z)));

    OutScreenTL = Bounds2D.Min;
    OutScreenTR = FVector2D(Bounds2D.Max.X, Bounds2D.Min.Y);
    OutScreenBR = Bounds2D.Max;
    OutScreenBL = FVector2D(Bounds2D.Min.X, Bounds2D.Max.Y);
}

void AGSHUD::ProjectBoundsBySphereRadius(
    const FVector& Origin,
    float SphereRadius,
    FVector2D& OutScreenTL,
    FVector2D& OutScreenTR,
    FVector2D& OutScreenBR,
    FVector2D& OutScreenBL
    )
{
    ProjectBoundsByExtents(
        Origin,
        FVector(SphereRadius),
        OutScreenTL,
        OutScreenTR,
        OutScreenBR,
        OutScreenBL
        );
}

// 

#include "AI/GSEnvQueryGenerator_ConeBetween.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UGSEnvQueryGenerator_ConeBetween::UGSEnvQueryGenerator_ConeBetween(const FObjectInitializer& ObjectInitializer) 
    : Super(ObjectInitializer)
{
    FromContext = UEnvQueryContext_Querier::StaticClass();
    ToContext = UEnvQueryContext_Querier::StaticClass();
    AlignedPointsDistance.DefaultValue = 100.f;
    ConeDegrees.DefaultValue = 90.f;
    AngleStep.DefaultValue = 10.f;
    MinRange.DefaultValue = 0.f;
    MaxRange.DefaultValue = 1000.f;
    bIncludeContextLocation = false;
    bIncludeAllFromContextActors = false;
}

void UGSEnvQueryGenerator_ConeBetween::BindDataToDataProviders(FEnvQueryInstance& QueryInstance) const
{
    //Bind the necessary data to data providers
    UObject* BindOwner = QueryInstance.Owner.Get();
    AlignedPointsDistance.BindData(BindOwner, QueryInstance.QueryID);
    ConeDegrees.BindData(BindOwner, QueryInstance.QueryID);
    AngleStep.BindData(BindOwner, QueryInstance.QueryID);
    MinRange.BindData(BindOwner, QueryInstance.QueryID);
    MaxRange.BindData(BindOwner, QueryInstance.QueryID);
}

void UGSEnvQueryGenerator_ConeBetween::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
    const float ConeDegreesValue = FMath::Clamp(FMath::Abs(ConeDegrees.GetValue()), 0.f, 359.f);
    if (ConeDegreesValue == 0)
    {
        return;
    }

    TArray<AActor*> FromActors;
    TArray<AActor*> ToActors;

    QueryInstance.PrepareContext(FromContext, FromActors);
    QueryInstance.PrepareContext(ToContext, ToActors);

    if (FromActors.Num() <= 0 || ToActors.Num() <= 0)
    {
        return;
    }

    const AActor* FromActor = FromActors[0];
    const FVector FromLocation = FromActor->GetActorLocation();

    BindDataToDataProviders(QueryInstance);
    
    //Get the values from each data provider
    const float AlignedPointDistanceValue = AlignedPointsDistance.GetValue();
    const float AngleStepValue = FMath::Clamp(AngleStep.GetValue(), 1.f, 359.f);
    const float GenerationRange = FMath::Clamp(MaxRange.GetValue(), 0.f, MAX_flt);
    const float MinGenerationRange = FMath::Clamp(MinRange.GetValue(), 0.f, GenerationRange);
    const int32 MaxPointsPerAngleValue = FMath::FloorToInt(GenerationRange / AlignedPointDistanceValue);

    TArray<FNavLocation> GeneratedItems;
    GeneratedItems.Reserve(ToActors.Num() * FMath::CeilToInt(ConeDegreesValue / AngleStepValue) * MaxPointsPerAngleValue + 1);

    //Generate points for each actor
    for (int32 CenterIndex = 0; CenterIndex < ToActors.Num(); CenterIndex++)
    {
        const FVector ActorLocation = ToActors[CenterIndex]->GetActorLocation();
        const FVector ForwardVector = (FromLocation-ActorLocation).GetSafeNormal();
        
        for (float Angle = -(ConeDegreesValue * 0.5f); Angle < (ConeDegreesValue * 0.5f); Angle += AngleStepValue)
        {
            const FVector AxisDirection = ForwardVector.RotateAngleAxis(Angle, FVector::UpVector);

            // skipping PointIndex == 0 as that's the context's location
            for (int32 PointIndex = 1; PointIndex <= MaxPointsPerAngleValue; PointIndex++)
            {
                const float PointDist = PointIndex * AlignedPointDistanceValue;

                // Only add points further than minimum generation range
                if (PointDist >= MinGenerationRange)
                {
                    const FVector GeneratedLocation = ActorLocation + (AxisDirection * PointDist);
                    GeneratedItems.Add(FNavLocation(GeneratedLocation));
                }
            }
        }

        if (bIncludeContextLocation)
        {
            GeneratedItems.Add(FNavLocation(ActorLocation));
        }
    }   

    ProjectAndFilterNavPoints(GeneratedItems, QueryInstance);
    StoreNavPoints(GeneratedItems, QueryInstance);
}

FText UGSEnvQueryGenerator_ConeBetween::GetDescriptionTitle() const
{
    return FText::Format(
        LOCTEXT(
            "ConeBetweenDescriptionGenerateAroundContext",
            "{0}: generate in front of {1}"
            ),
        Super::GetDescriptionTitle(),
        UEnvQueryTypes::DescribeContext(FromContext)
        );
}

FText UGSEnvQueryGenerator_ConeBetween::GetDescriptionDetails() const
{
    FText Desc = FText::Format(
        LOCTEXT(
            "ConeBetweenDescription",
            "degrees: {0}, angle step: {1}"
            ),
        FText::FromString(ConeDegrees.ToString()),
        FText::FromString(AngleStep.ToString())
        );

    FText ProjDesc = ProjectionData.ToText(FEnvTraceData::Brief);
    if (!ProjDesc.IsEmpty())
    {
        FFormatNamedArguments ProjArgs;
        ProjArgs.Add(TEXT("Description"), Desc);
        ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
        Desc = FText::Format(
            LOCTEXT(
                "ConeBetweenDescriptionWithProjection",
                "{Description}, {ProjectionDescription}"
                ),
            ProjArgs
            );
    }

    return Desc;
}

#undef LOCTEXT_NAMESPACE

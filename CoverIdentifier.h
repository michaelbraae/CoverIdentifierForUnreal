#pragma once
#include "CombatBehaviour.h"

#include "CoverIdentifier.generated.h"

UCLASS( ClassGroup=(NPC), meta=(BlueprintSpawnableComponent) )
class UCoverIdentifier : public UActorComponent
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	
	UPROPERTY()
	UCombatBehaviour* CombatBehaviour;

	UFUNCTION(BlueprintCallable)
	void IdentifyCover();

	TMap<FVector, TMap<FVector, float>> GroupedCoverSpots;

private:
	bool PerformOverlapBoxTrace(const UWorld* World, const FVector& Start, const FVector& End, TArray<FOverlapResult>& OutOverlaps) const;

	static FVector GetClosestFaceCenter(UStaticMeshComponent* StaticMeshComponent, const FVector& Point, FBox& ClosestFace, float SuppliedDepth, TMap<FVector, float>& CoverSpots, FVector TargetLocation);

	static TMap<FVector, float> GetCoverSpots(const FVector& CoverOrigin, const FVector& LateralDirection, const FTransform& ComponentTransform, const float CoverWidth, FVector TargetLocation);
	
	static FBox GetComponentOrientedBoundingBox(const USceneComponent* Component);
};

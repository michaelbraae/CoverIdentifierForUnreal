#include "CoverIdentifier.h"

#include "AIController.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void UCoverIdentifier::BeginPlay()
{
	Super::BeginPlay();

	CombatBehaviour = GetOwner()->FindComponentByClass<UCombatBehaviour>();

	if (AAIController* AIController = Cast<AAIController>(Cast<APawn>(GetOwner())->GetController()); !AIController)
	{
		AIController = GetWorld()->SpawnActor<AAIController>();
		AIController->Possess(Cast<APawn>(GetOwner()));
	}
}

void UCoverIdentifier::IdentifyCover()
{
	if (CombatBehaviour->CurrentTarget)
	{
		GroupedCoverSpots.Empty();
		UWorld* World = GetWorld();
		if (!World) return;
		
		FVector OwnerLocation = GetOwner()->GetActorLocation();
		FVector CurrentTargetLocation = CombatBehaviour->CurrentTarget->GetActorLocation();

		if (TArray<FOverlapResult> OutOverlaps; PerformOverlapBoxTrace(World, OwnerLocation, CurrentTargetLocation, OutOverlaps))
		{
			// get an array of static meshes that are between the NPC and it's target
			TArray<AStaticMeshActor*> CoverMeshes;
			for (const FOverlapResult& OverlapResult : OutOverlaps)
			{
				if (OverlapResult.GetActor() && OverlapResult.GetActor()->IsA(AStaticMeshActor::StaticClass()))
				{
					AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(OverlapResult.GetActor());

					// if we want to trim meshes that are further away than the player, uncomment this
					// FVector MeshActorLocation;
					// MeshActor->GetStaticMeshComponent()->GetClosestPointOnCollision(OwnerLocation, MeshActorLocation);
					// if (FVector::Dist(MeshActorLocation, CurrentTargetLocation) > FVector::Dist(OwnerLocation, CurrentTargetLocation)) continue;
					
					if (MeshActor && !CoverMeshes.Contains(MeshActor))
					{
						CoverMeshes.Add(MeshActor);
					}
				}
			}

			// iterate over each mesh and identify the best cover spots
			for (int i = 0; i < CoverMeshes.Num(); i++)
			{
				AStaticMeshActor* MeshActor = CoverMeshes[i];
				UStaticMeshComponent* StaticMeshComponent = MeshActor->GetStaticMeshComponent();
				
				FBox ClosestFace;
				TMap<FVector, float> CoverSpots;
				FVector ClosestFaceCenter = GetClosestFaceCenter(StaticMeshComponent, OwnerLocation, ClosestFace, 100, CoverSpots, CurrentTargetLocation);

				GroupedCoverSpots.Add(ClosestFaceCenter, CoverSpots);
				
				FTransform ActorTransform = MeshActor->GetActorTransform();
				
				FBox OrientedBoundingBox = GetComponentOrientedBoundingBox(MeshActor->GetStaticMeshComponent());
				
				// draw a debug box around the identified static mesh
				DrawDebugBox(World, OrientedBoundingBox.GetCenter(), OrientedBoundingBox.GetExtent(), ActorTransform.GetRotation(), FColor::Green, false, 0.0f, 0, 5.0f);
				
				// draw a debug box around the valid cover zone
				DrawDebugBox(World, ClosestFaceCenter, ClosestFace.GetExtent(), ActorTransform.GetRotation(), FColor::Yellow, false, 0.0f, 0, 5.0f);
				
				for (auto& CoverSpot : CoverSpots)
				{
					float Alpha = FMath::Clamp(CoverSpot.Value, 0.0f, 1.0f);

					// draw a debug sphere at each of the valid cover spots
					DrawDebugSphere(GetWorld(), CoverSpot.Key, 100.0f, 32, FMath::Lerp(FLinearColor::Red, FLinearColor::Green, Alpha).ToFColor(true), false, 0.0f);
				}
			}
		}
		else
		{
			DrawDebugLine(World, OwnerLocation, CurrentTargetLocation, FColor::Yellow, false, 1.0f);
		}
	}
}

/**
 *	Gets the center of the face on a given static mesh that is most openly facing the Point
 *	using the normal of the face compared to the dot product of the vector to the point
 * 
 * @param StaticMeshComponent 
 * @param Point 
 * @return 
 */
FVector UCoverIdentifier::GetClosestFaceCenter(UStaticMeshComponent* StaticMeshComponent, const FVector& Point, FBox& ClosestFace, float SuppliedDepth, TMap<FVector, float>& CoverSpots, FVector TargetLocation)
{
	if (!StaticMeshComponent)
    {
        return FVector::ZeroVector;
    }
	
    // Get the local bounding box of the static mesh
    FBox LocalBoundingBox = StaticMeshComponent->GetStaticMesh()->GetBoundingBox();
	
    // Calculate centers of all 6 faces of the bounding box in local space
    FVector LocalFaceCenters[6];
    LocalFaceCenters[0] = FVector((LocalBoundingBox.Min.X + LocalBoundingBox.Max.X) * 0.5f, LocalBoundingBox.Min.Y, (LocalBoundingBox.Min.Z + LocalBoundingBox.Max.Z) * 0.5f); // Min Y face
    LocalFaceCenters[1] = FVector((LocalBoundingBox.Min.X + LocalBoundingBox.Max.X) * 0.5f, LocalBoundingBox.Max.Y, (LocalBoundingBox.Min.Z + LocalBoundingBox.Max.Z) * 0.5f); // Max Y face
    LocalFaceCenters[2] = FVector(LocalBoundingBox.Min.X, (LocalBoundingBox.Min.Y + LocalBoundingBox.Max.Y) * 0.5f, (LocalBoundingBox.Min.Z + LocalBoundingBox.Max.Z) * 0.5f); // Min X face
    LocalFaceCenters[3] = FVector(LocalBoundingBox.Max.X, (LocalBoundingBox.Min.Y + LocalBoundingBox.Max.Y) * 0.5f, (LocalBoundingBox.Min.Z + LocalBoundingBox.Max.Z) * 0.5f); // Max X face
    LocalFaceCenters[4] = FVector((LocalBoundingBox.Min.X + LocalBoundingBox.Max.X) * 0.5f, (LocalBoundingBox.Min.Y + LocalBoundingBox.Max.Y) * 0.5f, LocalBoundingBox.Min.Z); // Min Z face
    LocalFaceCenters[5] = FVector((LocalBoundingBox.Min.X + LocalBoundingBox.Max.X) * 0.5f, (LocalBoundingBox.Min.Y + LocalBoundingBox.Max.Y) * 0.5f, LocalBoundingBox.Max.Z); // Max Z face
	
    // Face normals in local space
    FVector LocalFaceNormals[6];
    LocalFaceNormals[0] = FVector(0, -1, 0); // Min Y normal
    LocalFaceNormals[1] = FVector(0, 1, 0);  // Max Y normal
    LocalFaceNormals[2] = FVector(-1, 0, 0); // Min X normal
    LocalFaceNormals[3] = FVector(1, 0, 0);  // Max X normal
    LocalFaceNormals[4] = FVector(0, 0, -1); // Min Z normal
    LocalFaceNormals[5] = FVector(0, 0, 1);  // Max Z normal
	
    // Transform face centers and normals to world space
    FVector WorldFaceCenters[6];
    FVector WorldFaceNormals[6];
    FTransform ComponentTransform = StaticMeshComponent->GetComponentTransform();
	
    for (int32 i = 0; i < 6; ++i)
    {
        WorldFaceCenters[i] = ComponentTransform.TransformPosition(LocalFaceCenters[i]);
        WorldFaceNormals[i] = ComponentTransform.TransformVectorNoScale(LocalFaceNormals[i]);
    }
	
    // Find the closest face center considering orientation
    FVector ClosestCenter = WorldFaceCenters[0];
    float MinWeightedDistance = TNumericLimits<float>::Max();
    int32 ClosestFaceIndex = 0;
	
    for (int32 i = 0; i < 6; ++i)
    {
        FVector DirectionToPoint = (Point - WorldFaceCenters[i]).GetSafeNormal();
        float DotProduct = FVector::DotProduct(WorldFaceNormals[i], DirectionToPoint);
        float Distance = FVector::Dist(WorldFaceCenters[i], Point);
        float WeightedDistance = Distance * (1.0f - DotProduct);

    	// if the face is openly facing the target, skip it!
		if (FVector::DotProduct(WorldFaceNormals[i], WorldFaceCenters[i] - TargetLocation) < 0.0) continue;
    	
        if (WeightedDistance < MinWeightedDistance)
        {
            MinWeightedDistance = WeightedDistance;
            ClosestCenter = WorldFaceCenters[i];
            ClosestFaceIndex = i;
        }
    }
	
    // Set the ClosestFace argument
    FVector FaceCenter = LocalFaceCenters[ClosestFaceIndex];
    FVector BoxExtent;
	
    switch (ClosestFaceIndex)
    {
        case 0: // Min Y face
        	ClosestCenter -= ComponentTransform.TransformVectorNoScale(FVector(0, SuppliedDepth, 0));
        	BoxExtent = FVector(LocalBoundingBox.GetExtent().X, SuppliedDepth, LocalBoundingBox.GetExtent().Z);
    		BoxExtent.X *= ComponentTransform.GetScale3D().X;
    		BoxExtent.Z *= ComponentTransform.GetScale3D().Z;

    		CoverSpots = GetCoverSpots(
    			ClosestCenter,
				FVector(1, 0, 0),
				ComponentTransform,
				BoxExtent.X,
				TargetLocation
			);
			break;
        case 1: // Max Y face
        	ClosestCenter += ComponentTransform.TransformVectorNoScale(FVector(0, SuppliedDepth, 0));
    		BoxExtent = FVector(LocalBoundingBox.GetExtent().X, SuppliedDepth, LocalBoundingBox.GetExtent().Z);
    		BoxExtent.X *= ComponentTransform.GetScale3D().X;
    		BoxExtent.Z *= ComponentTransform.GetScale3D().Z;
    		CoverSpots = GetCoverSpots(
				ClosestCenter,
				FVector(1, 0, 0),
				ComponentTransform,
				BoxExtent.X,
				TargetLocation
			);
            break;
        case 2: // Min X face
        	ClosestCenter -= ComponentTransform.TransformVectorNoScale(FVector(SuppliedDepth, 0, 0));
    		BoxExtent = FVector(SuppliedDepth, LocalBoundingBox.GetExtent().Y, LocalBoundingBox.GetExtent().Z);
    		BoxExtent.Y *= ComponentTransform.GetScale3D().Y;
    		BoxExtent.Z *= ComponentTransform.GetScale3D().Z;
    		CoverSpots = GetCoverSpots(
				ClosestCenter,
				FVector(0, 1, 0),
				ComponentTransform,
				BoxExtent.Y,
				TargetLocation
			);
			break;
        case 3: // Max X face
        	ClosestCenter += ComponentTransform.TransformVectorNoScale(FVector(SuppliedDepth, 0, 0));
            BoxExtent = FVector(SuppliedDepth, LocalBoundingBox.GetExtent().Y, LocalBoundingBox.GetExtent().Z);
    		BoxExtent.Y *= ComponentTransform.GetScale3D().Y;
    		CoverSpots = GetCoverSpots(
				ClosestCenter,
				FVector(0, 1, 0),
				ComponentTransform,
				BoxExtent.Y,
				TargetLocation
			);
			BoxExtent.Z *= ComponentTransform.GetScale3D().Z;
    		break;
        case 4: // Min Z face
        case 5: // Max Z face
            BoxExtent = FVector(LocalBoundingBox.GetExtent().X, LocalBoundingBox.GetExtent().Y, SuppliedDepth);
            break;
    }
	
    ClosestFace = FBox(FaceCenter - BoxExtent, FaceCenter + BoxExtent);
	
    return ClosestCenter;
}

/**
 *	Gets a number of FVectors with a relevant value which determines their distance to the target, 0.0 -> 1.0, where 1.0 is the closest of the group
 *	if the cover width is less than 100 units, we just return the center with 1.0
 * 
 * @param CoverOrigin 
 * @param LateralDirection 
 * @param ComponentTransform 
 * @param CoverWidth 
 * @param TargetLocation 
 * @return 
 */
TMap<FVector, float> UCoverIdentifier::GetCoverSpots(const FVector& CoverOrigin, const FVector& LateralDirection, const FTransform& ComponentTransform, const float CoverWidth, FVector TargetLocation)
{
	constexpr float CoverMinWidth = 100;
	
	TMap<FVector, float> CoverSpots;
	
	CoverSpots.Add(CoverOrigin, 1.0);
	
	if (CoverWidth <= CoverMinWidth)
	{
		return CoverSpots;
	}

	const float CoverPartitions = FMath::Floor(CoverWidth / CoverMinWidth);
	
	const float Separation = CoverWidth / CoverPartitions;
	const FVector NormalizedLateralDirection = ComponentTransform.TransformVectorNoScale(LateralDirection.GetSafeNormal());
	
	for (int i = 1; i <= FMath::FloorToInt(CoverPartitions / 2); ++i)
	{
		FVector Offset = NormalizedLateralDirection + NormalizedLateralDirection * Separation * i;

		CoverSpots.Add(CoverOrigin + Offset * 2, 1.0);
		CoverSpots.Add(CoverOrigin - Offset * 2, 1.0);
	}

	// 1: Create an array with distances from the target location
	TArray<TPair<FVector, float>> DistanceArray;
	for (const TPair<FVector, float>& Pair : CoverSpots)
	{
		float Distance = FVector::Dist(Pair.Key, TargetLocation);
		DistanceArray.Add(TPair<FVector, float>(Pair.Key, Distance));
	}

	// 2: Determine the closest edge vector (last or second-to-last element)
	FVector ClosestEdgeVector = DistanceArray[DistanceArray.Num() - 2].Value > DistanceArray[DistanceArray.Num() - 1].Value  
		? DistanceArray[DistanceArray.Num() - 2].Key 
		: DistanceArray[DistanceArray.Num() - 1].Key;
	
	// 3: Recalculate distances based on the closest edge vector
	for (auto& Pair : DistanceArray)
	{
		Pair.Value = FVector::Dist(Pair.Key, ClosestEdgeVector);
	}
	
	// 4: Sort the array based on updated distances from the edge vector
	DistanceArray.Sort([](const TPair<FVector, float>& A, const TPair<FVector, float>& B) {
		return A.Value < B.Value;
	});
	
	// 5: Assign normalized float values based on sorted distances
	int32 NumElements = DistanceArray.Num();
	for (int32 Index = 0; Index < NumElements; ++Index)
	{
		float NormalizedValue = static_cast<float>(Index) / static_cast<float>(NumElements - 1);
		CoverSpots[DistanceArray[Index].Key] = NormalizedValue;
	}
	
	return CoverSpots;
};

/**
 *	Get a bounding box which is appropriately scaled to match the scene component
 * 
 * @param Component 
 * @return 
 */
FBox UCoverIdentifier::GetComponentOrientedBoundingBox(const USceneComponent* Component)
{
	FVector Origin;
	
	FVector ComponentExtent;
	float ComponentRadius;
	UKismetSystemLibrary::GetComponentBounds(Component, Origin, ComponentExtent, ComponentRadius);
	
	// Calculate the right extent
	const auto Box = Component->GetOwner()->CalculateComponentsBoundingBoxInLocalSpace(); 
	FVector Extent = Box.GetExtent();
	
	// Get the component's world scale
	FVector WorldScale = Component->GetComponentTransform().GetScale3D();
	
	// Scale the extent to match the component in world space
	Extent *= WorldScale;
	
	FBox ComponentOrientedBox(Origin - Extent, Origin + Extent);
	
	return ComponentOrientedBox;
}

/**
 * Performs the overlap trace between two location, looking for only static meshes
 * 
 * @param World 
 * @param Start 
 * @param End 
 * @param TraceChannel 
 * @param QueryParams 
 * @param ResponseParams 
 * @param OutOverlaps 
 * @return 
 */
bool UCoverIdentifier::PerformOverlapBoxTrace(const UWorld* World, const FVector& Start, const FVector& End, TArray<FOverlapResult>& OutOverlaps) const
{
	if (!World)
	{
		return false;
	}
	
	const FVector BoxExtents(FVector::Dist(Start, End) / 2, 500, 100);
	
	const FCollisionShape CollisionShape = FCollisionShape::MakeBox(BoxExtents);
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	
	FCollisionResponseParams ResponseParams;
	
	const FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(Start, End);
	
	bool bHit = World->OverlapMultiByChannel(
		OutOverlaps,
		(End + Start) / 2,
		Rotator.Quaternion(),
		ECC_WorldStatic,
		CollisionShape,
		QueryParams,
		ResponseParams
	);
	
	DrawDebugBox(World, (End + Start) / 2, BoxExtents, Rotator.Quaternion(), FColor::Blue, false, 0.0f);
	DrawDebugLine(World, Start, End, FColor::Blue, false, 0.0f);
	
	return bHit;
}


#include "CustomSerialization.h"
#include "MapSyncPrivatePCH.h"

#include "Engine/Light.h"

// Ar.IsLoading() means that we feed binary data into actor (when an actor is loading from disk)
// When it's false, it's the opposite: the actor is getting serialized into actor (when an actor is being saved to disk)

TSubclassOf<UObject> UCustomSerializer::GetSupportedClass() const {	return UObject::StaticClass(); }
void UCustomSerializer::MapSyncSerialize(FArchive& Ar, UObject* Obj) const
{
}

TSubclassOf<UObject> UCustomSerializerActor::GetSupportedClass() const { return AActor::StaticClass(); }
void UCustomSerializerActor::MapSyncSerialize(FArchive& Ar, UObject* Obj) const
{
	AActor* Actor = Cast<AActor>(Obj);
	if (!Actor) return;

	// If loading data from binary to actor
	if (Ar.IsLoading())
	{
		FVector Location;
		FRotator Rotation;
		FVector Scale;

		Ar << Location;
		Ar << Rotation;
		Ar << Scale;

		Actor->SetActorLocation(Location);
		Actor->SetActorRotation(Rotation);
		Actor->SetActorScale3D(Scale);
	}
	else
	{
		auto Location = Actor->GetActorLocation();
		auto Rotation = Actor->GetActorRotation();
		auto Scale = Actor->GetActorScale3D();

		Ar << Location;
		Ar << Rotation;
		Ar << Scale;
	}
}

TSubclassOf<UObject> UCustomSerializerSMActor::GetSupportedClass() const { return AStaticMeshActor::StaticClass(); }
void UCustomSerializerSMActor::MapSyncSerialize(FArchive& Ar, UObject* Obj) const
{
	auto* Actor = Cast<AStaticMeshActor>(Obj);
	if (!Actor) return;

	auto* SMComponent = Actor->GetStaticMeshComponent();
	if (!SMComponent) return;

	// Mesh
	if (Ar.IsLoading())
	{
		FString StaticMeshName;
		Ar << StaticMeshName;

		UStaticMesh* FoundMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), Cast<AActor>(Obj), *StaticMeshName));
		if (FoundMesh)
		{
			EComponentMobility::Type OldMobility = Cast<AStaticMeshActor>(Obj)->GetStaticMeshComponent()->Mobility;
			Actor->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(FoundMesh);
			Actor->SetMobility(OldMobility);
		}
	}
	else
	{
		FString StaticMeshName = FStringAssetReference(SMComponent->GetStaticMesh()).ToString();
		Ar << StaticMeshName;
	}

	// Materials
	if (Ar.IsLoading())
	{
		int32 MaterialCount;
		Ar << MaterialCount;

		for (int32 i = 0; i < MaterialCount; i++)
		{
			FString MaterialName;
			Ar << MaterialName;

			UMaterial* FoundMat = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), Cast<AActor>(Obj), *MaterialName));
			if (FoundMat)
			{
				SMComponent->SetMaterial(i, FoundMat);
			}
		}
	}
	else
	{
		int32 MaterialCount = SMComponent->GetNumMaterials();
		Ar << MaterialCount;
		for (int32 i = 0; i < MaterialCount; i++)
		{
			FString MaterialStr = FStringAssetReference(SMComponent->GetMaterial(i)->GetMaterial()).ToString();
			Ar << MaterialStr;
		}
	}
}

TSubclassOf<UObject> UCustomSerializerLightActor::GetSupportedClass() const { return ALight::StaticClass(); }
void UCustomSerializerLightActor::MapSyncSerialize(FArchive& Ar, UObject* Obj) const
{
	auto* Actor = Cast<ALight>(Obj);
	if (!Actor) return;

	auto* LightComponent = Actor->GetLightComponent();
	if (!LightComponent) return;

	// Serialize light color
	if (Ar.IsLoading()) // If Ar into actor
	{
		FLinearColor Temp;
		Ar << Temp;
		LightComponent->SetLightColor(Temp);
	}
	else // If actor into Ar
	{
		FLinearColor Temp = LightComponent->GetLightColor();
		Ar << Temp;
	}

	// Serialize light intensity
	if (Ar.IsLoading()) // If Ar into actor
	{
		float Temp;
		Ar << Temp;
		LightComponent->SetIntensity(Temp);
	}
	else // If actor into Ar
	{
		float Temp = LightComponent->Intensity;
		Ar << Temp;
	}
}


#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "CustomSerialization.generated.h"


UCLASS()
class UCustomSerializer : public UObject
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UObject> GetSupportedClass() const;
	virtual void MapSyncSerialize(FArchive& Ar, UObject* Obj) const;
};

UCLASS()
class UCustomSerializerActor : public UCustomSerializer
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UObject> GetSupportedClass() const override;
	virtual void MapSyncSerialize(FArchive& Ar, UObject* Obj) const override;
};

UCLASS()
class UCustomSerializerSMActor : public UCustomSerializer
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UObject> GetSupportedClass() const override;
	virtual void MapSyncSerialize(FArchive& Ar, UObject* Obj) const override;
};

UCLASS()
class UCustomSerializerLightActor : public UCustomSerializer
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<UObject> GetSupportedClass() const override;
	virtual void MapSyncSerialize(FArchive& Ar, UObject* Obj) const override;
};

// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

#pragma once

#include "Runtime/CoreUObject/Public/UObject/Object.h"
#include "Editor/UnrealEd/Public/UnrealEd.h" 
#include "Editor/UnrealEd/Public/Editor.h"
#include "Runtime/Networking/Public/Networking.h"

#include <functional>
#include <chrono>

#define MAPSYNC_INI FPaths::ProjectPluginsDir() + TEXT("MapSync/Config/MapSync.ini")

#define UPDATE_DELAY 0.1f

#define RESYNC_HEADER 'r'
#define UPDATE_HEADER 'e'
#define EXIT_HEADER 'x'

#define CREATE_CMD 'c'
#define REMOVE_CMD 'r'
#define UPDATE_CMD 'u'
#define RENAME_CMD 'e'

#define BPCLASS_CREATEFLAG 'b'
#define CPPCLASS_CREATEFLAG 'c'

// #define INITIALBUNCH_HEADER 'i'
// #define TICKBUNCH_HEADER 't'

class FMapSyncEdMode;
class FPackage;
class UCustomSerializer;

/*
 * The class handling the editor mode of MapSync
 * Also contains most of the logic behind, there was no point in putting it inside another file
 * The structure of the sent data is [COMMAND][NAMESIZE][NAME][DATASIZE][DATA], and all modifications are concatenated. The command is 1 byte, the sizes are 4 bytes long
 * At the message's beginning, there is the level name, and the same of the string just before it: [LEVELNAMESIZE][LEVELNAME]
 */
class FMapSyncEdMode : public FEdMode
{
public:
	static const FEditorModeID EM_MapSyncEdModeId; // Use for EdMode
	static bool bIsMapSyncSerialization;

public:
	// ctor, dtor
	FMapSyncEdMode();
	virtual ~FMapSyncEdMode();

	// Used for EdMode
	virtual void Enter() override; // Called when the editor enters this EdMode
	virtual void Exit() override; // Called when the editor exits this EdMode
	bool UsesToolkits() const override; // Requiered to use a toolkit, i.e. a GUI in the EdMode panel
	void UpdateToolkit(int32 UIMode);

// TCP client related stuff
public:
	FSocket* ConnectionToServer; // The socket handling connection to server
	FIPv4Address ServerAdress;// The server adress, in the UE4 adress form
	bool bConnectedToServer;// Wether it's connected
	void ConnectToServerAdress(const FString& ServerAdress); // Function to set server adress from a string
	void ResyncClient();

// TCP server related stuff
public:
	FSocket* ServerBind; // The socket handling port binding
	TArray<FSocket*> Clients;
	FIPv4Address ThisServerAdress;// The server adress, in the UE4 adress form
	bool bBound;// Wether it's connected
	void BindToPort(int32 Port);

public:
	bool bTimerLambdaSet;
	float AccTimeSinceLastCall;
	void Cancel();
	void UpdateMapSync();
	void ResyncToClient(int32 ClientIdx);
	void DeserializeResync(FMemoryReader& Ar);

// Change handling related stuff
private:
	bool bActorInit; // Wether the actor list was initialized
	TArray<TPair<AActor*, FString>> LastActorsNames; // Actors names stored by actors, used to detect names changes. TODO: replace by TMap, for easier searching?
	TArray<TPair<AActor*, TArray<uint8>>> LastActorsData; // Last actor send data, to don't send the same update twice
	void BuildLastActorsNames();
	TArray<UCustomSerializer*> CustomSerializers;
	void BuildCustomSerializers();

	bool SerializeAllActorsChange(FMemoryWriter& Ar); // Function which will compute and send all actor changes	
	void SerializeOneActorMod(AActor* TheActor, FMemoryWriter& Ar);
	void DeserializeAllActorsChange(FMemoryReader& Ar); // Called directly when a string is received

private:

	// When receiving data from network, it can contain more than one request
	// Thus, it's organized to be in packets, in the format [data size][actual data]
	// ArraysToNetData turns data into something sendable across network, and stackable
	// NetDataToArrays turns something received from network into a mapsync request
	void AppendArraysToNetData(TArray<uint8>& InputArray, TArray<uint8>& OutNetData);
	void NetDataToArrays(TArray<uint8>& NetData, TArray<TArray<uint8>>& OutArrays);
};

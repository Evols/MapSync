// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

#include "MapSyncEdMode.h"
#include "MapSyncPrivatePCH.h"
#include "MapSyncEdModeToolkit.h"
#include "Editor/UnrealEd/Public/Toolkits/ToolkitManager.h"
#include "Runtime/Core/Public/Logging/MessageLog.h"
#include <string>

#include "CustomSerialization.h"

#define LOCTEXT_NAMESPACE "MapSyncEditor"

// EdMode name
const FEditorModeID FMapSyncEdMode::EM_MapSyncEdModeId = TEXT("EM_MapSync");
bool FMapSyncEdMode::bIsMapSyncSerialization = false;

// ctor
FMapSyncEdMode::FMapSyncEdMode()
	: ConnectionToServer(nullptr), ServerBind(nullptr)
{
	bBound = false;
	bConnectedToServer = false;
	bActorInit = false;
	bTimerLambdaSet = false;
	AccTimeSinceLastCall = 0.f;
}

// dtor
FMapSyncEdMode::~FMapSyncEdMode()
{
}

void FMapSyncEdMode::Enter()
{
	FEdMode::Enter();

	// Set the main loop timer
	if (!bTimerLambdaSet)
	{
		bTimerLambdaSet = true;
		GEditor->OnPostEditorTick().AddLambda([&](float DeltaTime)
		{
			AccTimeSinceLastCall += DeltaTime;
			if (AccTimeSinceLastCall >= UPDATE_DELAY)
			{
				AccTimeSinceLastCall = FMath::Fmod(AccTimeSinceLastCall, UPDATE_DELAY);
				UpdateMapSync();
			}
		});
	}
	
	// Create widget
	if (!Toolkit.IsValid() && UsesToolkits())
	{
		UpdateToolkit(0);
	}
}

void FMapSyncEdMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

bool FMapSyncEdMode::UsesToolkits() const
{
	return true;
}

void FMapSyncEdMode::UpdateToolkit(int32 UIMode)
{
	Toolkit = MakeShareable(new FMapSyncEdModeToolkit(UIMode));
	Toolkit->Init(Owner->GetToolkitHost());
	static_cast<FMapSyncEdModeToolkit*>(Toolkit.Get())->OwnerEdMode = this;
}

void FMapSyncEdMode::ConnectToServerAdress(const FString& StringAdress)
{
	if (bConnectedToServer || bBound)
	{
		return;
	}

	BuildLastActorsNames();
	BuildCustomSerializers();
	bActorInit = true;

	TArray<FString> IPPortStrs;
	StringAdress.ParseIntoArray(IPPortStrs, TEXT(":"));
	if (IPPortStrs.Num() < 2)
	{
		return;
	}

	FIPv4Address::Parse(*IPPortStrs[0], ServerAdress);
	ConnectionToServer = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

	TSharedRef<FInternetAddr> InetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InetAddr->SetIp(ServerAdress.Value);
	InetAddr->SetPort(FCString::Atoi(*IPPortStrs[1]));

	bConnectedToServer = ConnectionToServer->Connect(*InetAddr);
	if(!bConnectedToServer)
	{
		FMessageLog("PIE").Warning()->AddToken(FTextToken::Create(FText::FromString("MapSync was unable to connect to server !")));
		UE_LOG(LogMapSync, Warning, TEXT("MapSync was unable to connect to server ! Server ip: %s, port: %d"), *IPPortStrs[0], FCString::Atoi(*IPPortStrs[1]));
		return;
	}

	UE_LOG(LogMapSync, Log, TEXT("MapSync successfully connected to server !"));
}

void FMapSyncEdMode::ResyncClient()
{
	TArray<uint8> SerializedData;
	FMemoryWriter DataToSendAr(SerializedData, true);
	char Header = RESYNC_HEADER;
	DataToSendAr << Header;

	TArray<uint8> DataToSend;
	AppendArraysToNetData(SerializedData, DataToSend);
	int32 Sent = 0;
	ConnectionToServer->Send(DataToSend.GetData(), DataToSend.Num(), Sent);
}


void FMapSyncEdMode::BindToPort(int32 Port)
{
	if (bConnectedToServer || bBound)
	{
		return;
	}

	BuildLastActorsNames();
	BuildCustomSerializers();
	bActorInit = true;

	FIPv4Address::Parse(TEXT("0.0.0.0"), ThisServerAdress);
	FIPv4Endpoint Endpoint(ThisServerAdress, Port);
	ServerBind = FTcpSocketBuilder(TEXT("Server"))
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.Listening(16);

	bBound = (ServerBind != nullptr);
	if (!bBound)
	{
		FMessageLog("PIE").Warning()->AddToken(FTextToken::Create(FText::FromString("MapSync was unable to create a server !")));
		UE_LOG(LogMapSync, Warning, TEXT("MapSync was unable to create a server ! Server port: %d"), Port);
		return;
	}

	UE_LOG(LogMapSync, Log, TEXT("MapSync successfully created a server ! (Port: %d)"), Port);
}

void FMapSyncEdMode::Cancel()
{
	bActorInit = false;
	if (bConnectedToServer)
	{
		TArray<uint8> SerializedData;
		FMemoryWriter DataToSendAr(SerializedData, true);
		char Header = EXIT_HEADER;
		DataToSendAr << Header;

		TArray<uint8> DataToSend;
		AppendArraysToNetData(SerializedData, DataToSend);
		int32 Sent = 0;
		ConnectionToServer->Send(DataToSend.GetData(), DataToSend.Num(), Sent);

		ConnectionToServer->Close();
		bConnectedToServer = false;
	}
	if (bBound)
	{
		ServerBind->Close();
		bBound = false;
	}
}

void FMapSyncEdMode::UpdateMapSync()
{
	// If is connected to a server (aka either client or server)
	if (bActorInit && bConnectedToServer && ConnectionToServer->GetConnectionState() != SCS_ConnectionError && ConnectionToServer->GetConnectionState() != SCS_NotConnected)
	{
		// If has pending data to treat, retrieve and treat it
		uint32 ReceivedSize = 0;
		while (ConnectionToServer && ConnectionToServer->HasPendingData(ReceivedSize))
		{
			TArray<uint8> ReceivedData; ReceivedData.SetNum(ReceivedSize);
			int32 DataRead = 0;
			ConnectionToServer->Recv(ReceivedData.GetData(), ReceivedSize, DataRead, ESocketReceiveFlags::None);

			FString ToAdd;
			for (uint8 it : ReceivedData)
			{
				ToAdd += (char)it;
			}

			// Split received data into a "understandable" packets, and parse them, and send them to clients
			TArray<TArray<uint8>> Arrays; NetDataToArrays(ReceivedData, Arrays);
			for (auto& Array : Arrays)
			{
				FMemoryReader ReceivedDataAr(Array);
				char Header;
				ReceivedDataAr << Header;
				if (Header == UPDATE_HEADER)
				{
					DeserializeAllActorsChange(ReceivedDataAr);
				}
				else if (Header == RESYNC_HEADER)
				{
					DeserializeResync(ReceivedDataAr);
				}
				else if (Header == EXIT_HEADER)
				{
					ConnectionToServer->Close();
					bConnectedToServer = false;
					return;
				}
			}
		}

		// Send local changes!
		TArray<uint8> SerializedData;
		FMemoryWriter DataToSendAr(SerializedData, true);
		char Header = UPDATE_HEADER;
		DataToSendAr << Header;
		if (SerializeAllActorsChange(DataToSendAr))
		{
			TArray<uint8> DataToSend;
			AppendArraysToNetData(SerializedData, DataToSend);
			int32 Sent = 0;
			ConnectionToServer->Send(DataToSend.GetData(), DataToSend.Num(), Sent);
		}
	}

	// If is a server
	if (bBound && bActorInit)
	{
		// Remove clients that disconnected
		for (int32 i = Clients.Num() - 1; i >= 0; i--)
		{
			if (!Clients[i] || Clients[i]->GetConnectionState() == SCS_NotConnected || Clients[i]->GetConnectionState() == ESocketConnectionState::SCS_ConnectionError || Clients[i]->GetConnectionState() == ESocketConnectionState::SCS_NotConnected)
			{
				Clients[i]->Close();
				Clients.RemoveAt(i);

				UE_LOG(LogMapSync, Log, TEXT("A client disconnected"));
			}
		}

		// If has a pending client wanting to connect, accept it
		bool bHasPendingConnection;
		ServerBind->HasPendingConnection(bHasPendingConnection);
		while (bHasPendingConnection)
		{
			int32 NewClientIdx = Clients.Add(ServerBind->Accept(TEXT("client") + FString::FromInt(FMath::RandHelper(1000000))));
			UE_LOG(LogMapSync, Log, TEXT("A client connected to this server: %s"), *Clients[NewClientIdx]->GetDescription());

			ServerBind->HasPendingConnection(bHasPendingConnection);
		}

		// Array into which we'll store the things to send to everybody
		TArray<TPair<FSocket*, TArray<uint8>>> ArraysToMulticast;

		// Send to all clients what was just received, and parse it live
		for (int32 ClientSocketIdx = Clients.Num() - 1; ClientSocketIdx >= 0; ClientSocketIdx--)
		{
			FSocket* ClientSocket = Clients[ClientSocketIdx];

			uint32 ReceivedSize = 0;
			while (ClientSocket && ClientSocket->HasPendingData(ReceivedSize))
			{
				// Receive raw data, into ReceivedData
				TArray<uint8> ReceivedData; ReceivedData.SetNum(ReceivedSize);
				int32 DataRead = 0;
				ClientSocket->Recv(ReceivedData.GetData(), ReceivedSize, DataRead, ESocketReceiveFlags::None);
				
				// The received and pending data size have to be the same, otherwise, something might be wrong
				if (DataRead != ReceivedSize)
				{
					UE_LOG(LogMapSync, Warning, TEXT("DataRead != ReceivedSize (%d vs %d)"), DataRead, ReceivedSize);
				}

				// Split ReceivedData into a "understandable" packets (aka Arrays), and parse them, and send them to clients
				TArray<TArray<uint8>> Arrays; NetDataToArrays(ReceivedData, Arrays);
				for (auto& Array : Arrays)
				{
					FMemoryReader ReceivedDataAr(Array);
					char Header;
					ReceivedDataAr << Header;

					bool bShouldMulticast = true;
					if (Header == UPDATE_HEADER)
					{
						DeserializeAllActorsChange(ReceivedDataAr);
					}
					else if (Header == RESYNC_HEADER)
					{
						ResyncToClient(ClientSocketIdx);
					}
					else if (Header == EXIT_HEADER)
					{
						bShouldMulticast = false;
						ClientSocket->Close();
						Clients.RemoveAt(ClientSocketIdx);
					}

					if (bShouldMulticast)
					{
						ArraysToMulticast.Add(TPairInitializer<FSocket*, TArray<uint8>>(ClientSocket, Array));
					}
				}
			}
		}

		// Send the data we just received to all clients (except the one that sent it)
		for (int32 ClientSocketIdx = 0; ClientSocketIdx < Clients.Num(); ClientSocketIdx++)
		{
			TArray<uint8> ArrayToMulticast;
			for (auto& Array : ArraysToMulticast)
			{
				if (Array.Key != Clients[ClientSocketIdx])
				{
					AppendArraysToNetData(Array.Value, ArrayToMulticast);
				}
			}

			if (ArrayToMulticast.Num() > 0)
			{
				int32 Sent = 0;
				Clients[ClientSocketIdx]->Send(ArrayToMulticast.GetData(), ArrayToMulticast.Num(), Sent);
			}
		}

		// If has data to send because of local changes, send it
		TArray<uint8> SerializedData;
		FMemoryWriter DataToSendAr(SerializedData, true);
		char Header = UPDATE_HEADER;
		DataToSendAr << Header;
		if (SerializeAllActorsChange(DataToSendAr))
		{
			TArray<uint8> DataToSend;
			AppendArraysToNetData(SerializedData, DataToSend);
			for (int32 ClientSocketIdx = 0; ClientSocketIdx < Clients.Num(); ClientSocketIdx++)
			{
				int32 Sent = 0;
				Clients[ClientSocketIdx]->Send(DataToSend.GetData(), DataToSend.Num(), Sent);
			}
		}
	}
}

void FMapSyncEdMode::ResyncToClient(int32 ClientIdx)
{
	TArray<uint8> SerializedData;
	FMemoryWriter Ar(SerializedData, true);
	char Header = RESYNC_HEADER;
	Ar << Header;

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		// Serialize actor name
		FString ActorName = ActorIt->GetFName().ToString();
		Ar << ActorName;

		// Serialize actor creation -> If is a BP class
		if (ActorIt->GetClass()->GetClass()->IsChildOf<UBlueprintGeneratedClass>())
		{
			char CreateFlag = BPCLASS_CREATEFLAG;
			Ar << CreateFlag;

			FString Path = ActorIt->GetClass()->GetPathName();
			Ar << Path;
		}
		// If is a CPP class
		else
		{
			char CreateFlag = CPPCLASS_CREATEFLAG;
			Ar << CreateFlag;

			FString ClassName = ActorIt->GetClass()->GetFName().ToString();
			Ar << ClassName;
		}

		// Serialize actor update
		SerializeOneActorMod(*ActorIt, Ar);
	}

	const FText Title = LOCTEXT("ResyncServerTitle", "Resync");
	
	int32 Size = SerializedData.Num();
	FString TheNum = "";

	if (Size > 1073741824) TheNum = FString::Printf(TEXT("%4.2f GiB"), static_cast<float>(Size) / 1073741824.f);
	else if (Size > 1048576) TheNum = FString::Printf(TEXT("%4.2f MiB"), static_cast<float>(Size) / 1048576.f);
	else if (Size > 1024) TheNum = FString::Printf(TEXT("%4.2f kiB"), static_cast<float>(Size) / 1024.f);
	else TheNum = FString::Printf(TEXT("%d bytes"), Size);

	// Warn the user that this may result in too much data being sent
	FSuppressableWarningDialog::FSetupInfo SetupInfo(FText::FromString("Warning, this update will be " + TheNum + ". Proceed?"), Title, "Warning_ResyncTitle");
	SetupInfo.ConfirmText = LOCTEXT("ResyncServerYesButton", "Yes");
	SetupInfo.CancelText = LOCTEXT("ResyncServerNoButton", "No");
	SetupInfo.CheckBoxText = FText::GetEmpty();	// not suppressible

	FSuppressableWarningDialog ReparentBlueprintDlg(SetupInfo);
	if (ReparentBlueprintDlg.ShowModal() == FSuppressableWarningDialog::Confirm)
	{
		TArray<uint8> DataToSend;
		AppendArraysToNetData(SerializedData, DataToSend);
		int32 Sent = 0;
		Clients[ClientIdx]->Send(DataToSend.GetData(), DataToSend.Num(), Sent);
	}
}

void FMapSyncEdMode::DeserializeResync(FMemoryReader& Ar)
{
	while (!Ar.AtEnd())
	{
		// Deserialize vars
		FString ActorName; Ar << ActorName;
		char CreateFlag; Ar << CreateFlag;
		FString Path; Ar << Path;

		// Try to find the actor
		AActor* FoundActor = nullptr;
		for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
		{
			if (ActorIt->GetFName().ToString() == ActorName)
			{
				FoundActor = *ActorIt;
			}
		}
		// If we can't find it, create it
		if (!FoundActor)
		{
			if (CreateFlag == BPCLASS_CREATEFLAG)
			{
				UClass* FoundClass = LoadClass<AActor>(nullptr, *Path);
				if (FoundClass)
				{
					FActorSpawnParameters ASP;
					ASP.Name = FName(*ActorName);

					FoundActor = GetWorld()->SpawnActor<AActor>(FoundClass, ASP);
				}
			}
			else if (CreateFlag == CPPCLASS_CREATEFLAG)
			{
				for (TObjectIterator<UClass> It; It; ++It)
				{
					if ((*It) && It->GetFName().ToString() == Path)
					{
						FActorSpawnParameters ASP;
						ASP.Name = FName(*ActorName);

						FoundActor = GetWorld()->SpawnActor<AActor>(*It, ASP);
						break;
					}
				}
			}
		}

		// If an actor to modify is selected, unselect it
		for (FSelectionIterator SelectionIt = GEditor->GetSelectedActorIterator(); SelectionIt; ++SelectionIt)
		{
			if (FoundActor == *SelectionIt)
			{
				GEditor->GetSelectedActors()->Deselect(FoundActor);
				break;
			}
		}

		// Apply serializers
		for (auto& Serializer : CustomSerializers)
		{
			if (FoundActor->GetClass()->IsChildOf(Serializer->GetSupportedClass()) || FoundActor->GetClass() == Serializer->GetSupportedClass())
			{
				Serializer->MapSyncSerialize(Ar, FoundActor);
			}
		}
	}
}

void FMapSyncEdMode::BuildLastActorsNames()
{
	LastActorsNames.Empty();

	// Init LastActorMod -> Set all actors but not their descriptions, to avoid memory usage
	// Descriptions will be set as soon as they are selected
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		if (ActorIt->IsA(ULevel::StaticClass()) || ActorIt->IsA(ALevelScriptActor::StaticClass()))
		{
			continue;
		}

		LastActorsNames.Add(TPairInitializer<AActor*, FString>(*ActorIt, ActorIt->GetFName().ToString()));
	}
}

void FMapSyncEdMode::BuildCustomSerializers()
{
	CustomSerializers.Empty();

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UCustomSerializer::StaticClass()))
		{
			UCustomSerializer* CS = const_cast<UCustomSerializer*>(GetDefault<UCustomSerializer>(*It));
			CustomSerializers.Add(CS);
		}
	}

	// WTF UE4?! Why is it deferencing pointers?!
	CustomSerializers.Sort([](UCustomSerializer& First, UCustomSerializer& Second) { return First.GetFName().FastLess(Second.GetFName()); });
}

bool FMapSyncEdMode::SerializeAllActorsChange(FMemoryWriter& Ar)
{
	// Add level name at the beginning, so each client knows wether the data sent it for his level or not
	FString LevelName = GetWorld()->GetFName().ToString();
	Ar << LevelName;

	bool ToReturn = false;

	// Handle renamed actors
	for (auto& it : LastActorsNames)
	{
		AActor* TheActor = it.Key;
		if (!TheActor || TheActor->IsPendingKill())
		{
			continue;
		}

		if (TheActor->GetFName().ToString() != it.Value)
		{
			char Cmd = RENAME_CMD;
			Ar << Cmd;
			FString OldName = it.Value;
			Ar << OldName;
			FString NewName = TheActor->GetFName().ToString();
			Ar << NewName;
			ToReturn = true;

			it.Value = TheActor->GetFName().ToString();
		}
	}

	// Handle deleted actors
	for (int32 LActorIdx = LastActorsNames.Num() - 1; LActorIdx >= 0; LActorIdx--)
	{
		AActor* LActor = LastActorsNames[LActorIdx].Key;
		if (!LActor)
		{
			LastActorsNames.RemoveAt(LActorIdx);
		}
		else if (LActor->IsPendingKill())
		{
			char Cmd = REMOVE_CMD;
			Ar << Cmd;
			FString Name = LActor->GetFName().ToString();
			Ar << Name;
			ToReturn = true;

			LastActorsNames.RemoveAt(LActorIdx);
		}
	}

	// Handle created actors
	for (FSelectionIterator SelectionIt = GEditor->GetSelectedActorIterator(); SelectionIt; ++SelectionIt)
	{
		AActor* SelectedActor = Cast<AActor>(*SelectionIt);
		if (!SelectedActor || SelectedActor->IsPendingKill())
		{
			continue;
		}

		bool bFound = false;
		for (int32 LActorIdx = 0; LActorIdx < LastActorsNames.Num(); LActorIdx++)
		{
			AActor* LActor = LastActorsNames[LActorIdx].Key;
			if (!LActor || LActor->IsPendingKill())
			{
				continue;
			}

			if (LActor == SelectedActor)
			{
				bFound = true;
				break;
			}
		}

		// If the actor was not found in the LastActorsNames array, it was just created
		if (!bFound)
		{
			LastActorsNames.Add(TPairInitializer<AActor*, FString>(SelectedActor, SelectedActor->GetFName().ToString()));

			char Cmd = CREATE_CMD;
			Ar << Cmd;
			FString ActorName = SelectionIt->GetFName().ToString();
			Ar << ActorName;

			// If is a BP class
			if (SelectionIt->GetClass()->GetClass()->IsChildOf<UBlueprintGeneratedClass>())
			{
				char CreateFlag = BPCLASS_CREATEFLAG;
				Ar << CreateFlag;

				FString Path = SelectionIt->GetClass()->GetPathName();
				Ar << Path;
			}
			// If is a CPP class
			else
			{
				char CreateFlag = CPPCLASS_CREATEFLAG;
				Ar << CreateFlag;

				FString ClassName = SelectionIt->GetClass()->GetFName().ToString();
				Ar << ClassName;
			}
			ToReturn = true;
			// No need to serialize the actor here, it will be serialized later in the function
		}
	}

	// Handle actor modifications
	for (FSelectionIterator SelectionIt = GEditor->GetSelectedActorIterator(); SelectionIt; ++SelectionIt)
	{
		AActor* ActorToMod = Cast<AActor>(*SelectionIt); if (!ActorToMod) continue;

		// First, see if what we'll send isn't a duplicate
		// Serialize the actor into a temporary array
		TArray<uint8> TempActorArray;
		FMemoryWriter TempAr(TempActorArray, true);
		SerializeOneActorMod(ActorToMod, TempAr);

		// Find the previous data that were sent for this actor. If they don't exist, create them
		TPair<AActor*, TArray<uint8>>* FoundPair = LastActorsData.FindByPredicate([&](TPair<AActor*, TArray<uint8>>& TempPair)
		{
			return TempPair.Key == ActorToMod;
		});
		if (FoundPair == nullptr)
		{
			LastActorsData.Add(TPairInitializer<AActor*, TArray<uint8>>(ActorToMod, {}));
			FoundPair = &LastActorsData.Last();
		}

		// If the fount data were the same, don't send them
		if (FoundPair->Value == TempActorArray)
		{
			continue;
		}
		FoundPair->Value = TempActorArray;

		// Send the update
		char Cmd = UPDATE_CMD;
		Ar << Cmd;
		FString Name = SelectionIt->GetFName().ToString();
		Ar << Name;
		ToReturn = true;
		SerializeOneActorMod(ActorToMod, Ar);
	}

	return ToReturn;
}

void FMapSyncEdMode::SerializeOneActorMod(AActor* TheActor, FMemoryWriter& Ar)
{
	if (!TheActor) return;
	for (auto& Serializer : CustomSerializers)
	{
		if (TheActor->GetClass()->IsChildOf(Serializer->GetSupportedClass()))
		{
			Serializer->MapSyncSerialize(Ar, TheActor);
		}
	}
}

void FMapSyncEdMode::DeserializeAllActorsChange(FMemoryReader& Ar)
{
	// Check that the target level is the one we're editing
	FString LevelName;
	Ar << LevelName;
	if (LevelName != GetWorld()->GetFName().ToString())
	{
		// GEngine->AddOnScreenDebugMessage(-1, 500.f, FColor::Red, "EXIT " + GetWorld()->GetFName().ToString());
		return;
	}

	if (Ar.AtEnd())
	{
		return;
	}

	char NextCmd = '\0';
	bool bShouldContinue = true;
	while(!Ar.AtEnd() && bShouldContinue)
	{
		Ar << NextCmd;
		bShouldContinue = false;

		// Handle renamed actors
		if (NextCmd == RENAME_CMD)
		{
			bShouldContinue = true;

			FName OldName;
			Ar << OldName;
			FName NewName;
			Ar << NewName;

			for (auto& ActorNameIt : LastActorsNames)
			{
				if (ActorNameIt.Key->GetFName() == OldName)
				{
#if 0
					ActorNameIt.Key->Rename(*NewName.ToString());
					ActorNameIt.Value = NewName;
#endif
					break;
				}
			}

			if (Ar.AtEnd())
			{
				return;
			}
			continue;
		}

		// Handle removed actors
		if (NextCmd == REMOVE_CMD)
		{
			bShouldContinue = true;

			FString ActorName;
			Ar << ActorName;

			for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
			{
				if (ActorIt->GetFName().ToString() == ActorName)
				{
					// Remove the actor in LastActorsData
					for (int32 i = 0; i < LastActorsData.Num(); i++)
					{
						if (LastActorsData[i].Key == *ActorIt)
						{
							LastActorsData.RemoveAtSwap(i);
							break;
						}
					}
					// Remove the actor in LastActorsNames
					for (int32 i = 0; i < LastActorsNames.Num(); i++)
					{
						if (LastActorsNames[i].Key == *ActorIt)
						{
							LastActorsNames.RemoveAtSwap(i);
							break;
						}
					}
					// Actually destroy the actor
					ActorIt->Destroy();
					break;
				}
			}

			if (Ar.AtEnd())
			{
				return;
			}
			continue;
		}

		// Handle created actors
		if (NextCmd == CREATE_CMD)
		{
			bShouldContinue = true;

			FString ActorName;
			Ar << ActorName;
			
			char CreateFlag;
			Ar << CreateFlag;

			if (CreateFlag == BPCLASS_CREATEFLAG)
			{
				FString Path;
				Ar << Path;

				UClass* FoundClass = LoadClass<AActor>(nullptr, *Path);
				if (FoundClass)
				{
					FActorSpawnParameters ASP;
					ASP.Name = FName(*ActorName);

					GetWorld()->SpawnActor<AActor>(FoundClass, ASP);
				}
			}
			else if (CreateFlag == CPPCLASS_CREATEFLAG)
			{
				FString ClassName;
				Ar << ClassName;

				for (TObjectIterator<UClass> It; It; ++It)
				{
					if ((*It) && It->GetFName().ToString() == ClassName)
					{
						FActorSpawnParameters ASP;
						ASP.Name = FName(*ActorName);

						GetWorld()->SpawnActor<AActor>(*It, ASP);
						break;
					}
				}
			}

			if (Ar.AtEnd())
			{
				return;
			}
			continue;
		}

		// Handle actor modifications
		if (NextCmd == UPDATE_CMD)
		{
			bShouldContinue = true;

			FString ActorName;
			Ar << ActorName;

			for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
			{
				if (ActorIt->GetFName().ToString() == ActorName)
				{
					AActor* ActorToMod = *ActorIt;

					// If an actor to modify is selected, unselect it
					for (FSelectionIterator SelectionIt = GEditor->GetSelectedActorIterator(); SelectionIt; ++SelectionIt)
					{
						if (ActorToMod == *SelectionIt)
						{
							GEditor->GetSelectedActors()->Deselect(ActorToMod);
							break;
						}
					}

					for (auto& Serializer : CustomSerializers)
					{
						if (ActorToMod->GetClass()->IsChildOf(Serializer->GetSupportedClass()) || ActorToMod->GetClass() == Serializer->GetSupportedClass())
						{
							Serializer->MapSyncSerialize(Ar, ActorToMod);
						}
					}

					break;
				}
			}

			if (Ar.AtEnd())
			{
				break;
			}
			continue;
		}
	}
}

void FMapSyncEdMode::AppendArraysToNetData(TArray<uint8>& InputArray, TArray<uint8>& OutNetData)
{
	int32 BaseIdx = OutNetData.Num();
	OutNetData.AddUninitialized(sizeof(int32));

	*reinterpret_cast<int32*>(BaseIdx + OutNetData.GetData()) = InputArray.Num();
	OutNetData.Append(InputArray);
}

void FMapSyncEdMode::NetDataToArrays(TArray<uint8>& NetData, TArray<TArray<uint8>>& OutArrays)
{
	OutArrays.Empty();
	for (int32 CurrentIdx = 0; NetData.IsValidIndex(CurrentIdx + sizeof(int32)); )
	{
		int32 CurrentSize = *reinterpret_cast<int32*>(NetData.GetData() + CurrentIdx);
		OutArrays.Add(TArray<uint8>(NetData.GetData() + CurrentIdx + sizeof(int32), CurrentSize));

		CurrentIdx += CurrentSize + 4;
	}
}

#undef LOCTEXT_NAMESPACE

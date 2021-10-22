// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

#include "MapSync.h"
#include "MapSyncPrivatePCH.h"
#include "MapSyncEdMode.h"
#include "Runtime/Slate/Public/SlateBasics.h"
#include "Runtime/SlateCore/Public/Styling/SlateStyle.h"
#include "Editor/EditorStyle/Public/EditorStyleSet.h"
#include "Runtime/Projects/Public/Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FMapSyncModule"

DEFINE_LOG_CATEGORY(LogMapSync);
DEFINE_LOG_CATEGORY(LogMapSyncDebug);

void FMapSyncModule::StartupModule()
{
	UE_LOG(LogMapSync, Error, TEXT("StartupModule StartupModule StartupModule StartupModule"));

	FEditorModeRegistry::Get().RegisterMode<FMapSyncEdMode>(FMapSyncEdMode::EM_MapSyncEdModeId, LOCTEXT("MapSyncEdModeName", "MapSyncEdMode"), FSlateIcon(FEditorStyle::GetStyleSetName(), "DeviceDetails.Share"), true);
}

void FMapSyncModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FMapSyncEdMode::EM_MapSyncEdModeId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMapSyncModule, MapSync)

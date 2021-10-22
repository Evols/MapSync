// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Editor/UnrealEd/Public/Features/IPluginsEditorFeature.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMapSync, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMapSyncDebug, Log, All);

class FMapSyncModule : public IModuleInterface, public IPluginsEditorFeature
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};

// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

#pragma once

#include "Editor/UnrealEd/Public/Toolkits/BaseToolkit.h"

class FMapSyncEdMode;

class FMapSyncEdModeToolkit : public FModeToolkit
{
public:

	FMapSyncEdModeToolkit(int32 nUIMode);

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

	FMapSyncEdMode* OwnerEdMode;

private:

	TSharedPtr<SWidget> ToolkitWidget;

private:

	void PrintServerUI();
	void PrintClientUI();
	void PrintHelpUI();

	int32 UIMode;
	FString IPToConnectTo;
	int32 PortToBindTo;

	void OnClientValidated();
	void OnServerValidated();
	void Cancel();
	void OnClientResync();
};

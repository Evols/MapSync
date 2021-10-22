// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

#include "MapSyncEdModeToolkit.h"
#include "MapSyncPrivatePCH.h"
#include "MapSyncEdMode.h"
#include "SMapSyncMenu.h"

#define LOCTEXT_NAMESPACE "FMapSyncEdModeToolkit"

FMapSyncEdModeToolkit::FMapSyncEdModeToolkit(int32 nUIMode)
{
	UIMode = nUIMode;

	if (nUIMode == 0)
	{
		PrintClientUI();
	}
	else if (nUIMode == 1)
	{
		PrintServerUI();
	}
	else if (nUIMode == 2)
	{
		PrintHelpUI();
	}
}

FName FMapSyncEdModeToolkit::GetToolkitFName() const
{
	return FName("MapSyncEdMode");
}

FText FMapSyncEdModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("MapSyncEdModeToolkit", "DisplayName", "MapSyncEdMode Tool");
}

class FEdMode* FMapSyncEdModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FMapSyncEdMode::EM_MapSyncEdModeId);
}

void FMapSyncEdModeToolkit::PrintServerUI()
{
	FString IniPort;
	if (GConfig)
	{
		GConfig->GetString(TEXT("MapSync"), TEXT("PortToBindTo"), IniPort, MAPSYNC_INI);
	}

	PortToBindTo = FCString::Atoi(*IniPort);

	struct Locals
	{
		static bool IsWidgetEnabled()
		{
			return true;
		}
	};

	SAssignNew(ToolkitWidget, SBorder)
	.VAlign(VAlign_Top)
	.Padding(FMargin(10.f, 0.f))
	.IsEnabled_Static(&Locals::IsWidgetEnabled)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(21.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Client mode"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(0);
					return FReply::Handled();
				})
				.IsEnabled_Lambda([&]()
				{
					return !OwnerEdMode->bBound;
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.FillWidth(22.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Server mode"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(1);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.FillWidth(10.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Help"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(2);
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString("Server port:"))
				.MinDesiredWidth(68.f)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SEditableTextBox)
				.MinDesiredWidth(40.f)
				.Text(FText::FromString(IniPort))
				.OnTextChanged_Lambda([&](const FText& NewText)
				{
					PortToBindTo = FCString::Atoi(*NewText.ToString());
				})
				.OnTextCommitted_Lambda([&](const FText& NewText, ETextCommit::Type TextCommit)
				{
					if (TextCommit == ETextCommit::OnEnter)
					{
						PortToBindTo = FCString::Atoi(*NewText.ToString());
						OnServerValidated();
					}
				})
				.IsEnabled_Lambda([&]()
				{
					return !OwnerEdMode->bBound;
				})
			]
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SButton)
			.Text(FText::FromString("Host server"))
			.OnClicked_Lambda([&]()
			{
				OnServerValidated();
				return FReply::Handled();
			})
			.IsEnabled_Lambda([&]()
			{
				return !OwnerEdMode->bBound;
			})
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SButton)
			.Text(FText::FromString("Cancel"))
			.OnClicked_Lambda([&]()
			{
				Cancel();
				return FReply::Handled();
			})
			.IsEnabled_Lambda([&]()
			{
				return OwnerEdMode->bBound;
			})
		]
	];
}

void FMapSyncEdModeToolkit::PrintClientUI()
{
	FString IniIp;
	if (GConfig)
	{
		GConfig->GetString(TEXT("MapSync"), TEXT("IPToConnectTo"), IniIp, MAPSYNC_INI);
	}

	IPToConnectTo = IniIp;

	struct Locals
	{
		static bool IsWidgetEnabled()
		{
			return true;
		}
	};
	
	SAssignNew(ToolkitWidget, SBorder)
	.VAlign(VAlign_Top)
	.Padding(FMargin(10.f, 0.f))
	.IsEnabled_Static(&Locals::IsWidgetEnabled)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(21.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Client mode"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(0);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.FillWidth(22.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Server mode"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(1);
					return FReply::Handled();
				})
				.IsEnabled_Lambda([&]()
				{
					return !OwnerEdMode->bConnectedToServer;
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.FillWidth(10.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Help"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(2);
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString("Server IP:"))
				.MinDesiredWidth(68.f)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SEditableTextBox)
				.MinDesiredWidth(100.f)
				.Text(FText::FromString(IniIp))
				.OnTextChanged_Lambda([&](const FText& NewText)
				{
					IPToConnectTo = NewText.ToString();
				})
				.OnTextCommitted_Lambda([&](const FText& NewText, ETextCommit::Type TextCommit)
				{
					if (TextCommit == ETextCommit::OnEnter)
					{
						IPToConnectTo = NewText.ToString();
						OnClientValidated();
					}
				})
				.IsEnabled_Lambda([&]()
				{
					return !OwnerEdMode->bConnectedToServer;
				})
			]
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SButton)
			.Text(FText::FromString("Connect to server"))
			.OnClicked_Lambda([&]()
			{
				OnClientValidated();
				return FReply::Handled();
			})
			.IsEnabled_Lambda([&]()
			{
				return !OwnerEdMode->bConnectedToServer;
			})
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SButton)
			.Text(FText::FromString("Cancel"))
			.OnClicked_Lambda([&]()
			{
				Cancel();
				return FReply::Handled();
			})
			.IsEnabled_Lambda([&]()
			{
				return OwnerEdMode->bConnectedToServer;
			})
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(FMargin(0.f, 16.f))
		[
			SNew(SButton)
			.Text(FText::FromString("Resync with server"))
			.OnClicked_Lambda([&]()
			{
				OnClientResync();
				return FReply::Handled();
			})
			.IsEnabled_Lambda([&]()
			{
				return OwnerEdMode->bConnectedToServer;
			})
		]
	];
}

void FMapSyncEdModeToolkit::PrintHelpUI()
{
	struct Locals
	{
		static bool IsWidgetEnabled()
		{
			return true;
		}
	};

	SAssignNew(ToolkitWidget, SBorder)
	.VAlign(VAlign_Top)
	.Padding(FMargin(10.f, 0.f))
	.IsEnabled_Static(&Locals::IsWidgetEnabled)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(21.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Client mode"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(0);
					return FReply::Handled();
				})
				.IsEnabled_Lambda([&]()
				{
					return !OwnerEdMode->bBound;
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.FillWidth(22.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Server mode"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(1);
					return FReply::Handled();
				})
				.IsEnabled_Lambda([&]()
				{
					return !OwnerEdMode->bConnectedToServer;
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.FillWidth(10.f)
			[
				SNew(SButton)
				.Text(FText::FromString("Help"))
				.OnClicked_Lambda([&]()
				{
					OwnerEdMode->UpdateToolkit(2);
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Hello, and welcome to the MapSync Plugin. This is the help section. To get started, please read the wiki page."))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Open wiki"))
				.OnClicked_Lambda([]()
				{
					UKismetSystemLibrary::LaunchURL("https://mapsync.wikia.com/wiki/Getting_started");
					return FReply::Handled();
				})
			]
		]
	];
}

void FMapSyncEdModeToolkit::OnClientValidated()
{
	GConfig->SetString(TEXT("MapSync"), TEXT("IPToConnectTo"), *IPToConnectTo, MAPSYNC_INI);
	GConfig->Flush(false, MAPSYNC_INI);

	OwnerEdMode->ConnectToServerAdress(IPToConnectTo);
}

void FMapSyncEdModeToolkit::OnServerValidated()
{
	GConfig->SetInt(TEXT("MapSync"), TEXT("PortToBindTo"), PortToBindTo, MAPSYNC_INI);
	GConfig->Flush(false, MAPSYNC_INI);

	OwnerEdMode->BindToPort(PortToBindTo);
}

void FMapSyncEdModeToolkit::Cancel()
{
	OwnerEdMode->Cancel();
}

void FMapSyncEdModeToolkit::OnClientResync()
{
	const FText Title = LOCTEXT("ResyncTitle", "Reload all data from server");
	const FText Message = LOCTEXT("ResyncWarning", "Level reloading might need a lot of data. Process?");

	// Warn the user that this may result in data loss
	FSuppressableWarningDialog::FSetupInfo Info(Message, Title, "Warning_ResyncTitle");
	Info.ConfirmText = LOCTEXT("ResyncYesButton", "Resync");
	Info.CancelText = LOCTEXT("ResyncNoButton", "Cancel");
	Info.CheckBoxText = FText::GetEmpty();	// not suppressible

	FSuppressableWarningDialog ReparentBlueprintDlg(Info);
	if (ReparentBlueprintDlg.ShowModal() == FSuppressableWarningDialog::Confirm)
	{
		OwnerEdMode->ResyncClient();
	}
}

#undef LOCTEXT_NAMESPACE

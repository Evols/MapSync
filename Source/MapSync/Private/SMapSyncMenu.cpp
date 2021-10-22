
#include "SMapSyncMenu.h"
#include "MapSyncPrivatePCH.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMapSyncMenu::Construct(const FArguments& InArgs)
{
	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.AddWidget(SNew(SButton)
		.Text(FText::FromString("MapSync"))
		, FText::GetEmpty());
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// Copyright 2018, Baptiste Hutteau. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MapSync : ModuleRules
{
    public MapSync(ReadOnlyTargetRules Target) 
        : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // PublicIncludePaths.AddRange(new string[] {"MapSync/Public"});
        PrivateIncludePaths.AddRange(new string[] {"MapSync/Private"});

		PublicDependencyModuleNames.AddRange(new string[] 
        {
            "Core",
            "Sockets",
            "Networking",
        });
        PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
            "EditorStyle",
            "LevelEditor",
            "EditorWidgets",
            "PropertyEditor",
        });
	}
}

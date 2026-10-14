// Copyright 2026 ue-mcp. All Rights Reserved.

using UnrealBuildTool;

public class UE_MCP_Studio : ModuleRules
{
	public UE_MCP_Studio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// scripts/release.mjs --fab sets this to true in the Fab copy; Fab updates that copy, never the plugin itself.
		const bool bFabBuild = false;
		PrivateDefinitions.Add("UE_MCP_STUDIO_FAB=" + (bFabBuild ? "1" : "0"));

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"WebBrowser",
			"Projects",
			"Json",
			"HTTP",
			"InputCore",
			"ApplicationCore",
			"ContentBrowser",
			"GraphEditor",
		});
	}
}

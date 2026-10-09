using UnrealBuildTool;

public class UE_MCP_Studio : ModuleRules
{
	public UE_MCP_Studio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"WebBrowser",
			"Projects",
			"Json",
			"HTTP",
		});
	}
}

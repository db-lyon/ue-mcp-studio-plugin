#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "SStudioPanel.h"
#include "StudioDaemon.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "UE_MCP_Studio"

DEFINE_LOG_CATEGORY_STATIC(LogStudioModule, Log, All);

namespace
{
	const FName StudioTab(TEXT("UEMCPStudio"));
}

class FStudioModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(StudioTab, FOnSpawnTab::CreateRaw(this, &FStudioModule::SpawnTab))
			.SetDisplayName(LOCTEXT("TabTitle", "UE-MCP Studio"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Agents, flows, setup and logs for ue-mcp"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings"));

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FStudioModule::RegisterMenus));

		// The daemon is the product; the tab is only a view. Start it with the editor, not the tab.
		FStudioDaemonInfo Info;
		if (!FStudioDaemon::ReadLive(Info) && FStudioDaemon::IsBridgeInstalled() && !FStudioDaemon::FindNode().IsEmpty())
		{
			FString Error;
			if (!FStudioDaemon::StartDaemon(Error))
			{
				UE_LOG(LogStudioModule, Warning, TEXT("daemon did not start: %s"), *Error);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(StudioTab);
		}
	}

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SStudioPanel)];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		FToolMenuSection& Section = Toolbar->FindOrAddSection("UEMCPStudio");
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"OpenUEMCPStudio",
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(StudioTab); })),
			LOCTEXT("ToolbarLabel", "Studio"),
			LOCTEXT("ToolbarTooltip", "Open UE-MCP Studio"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings")));
	}
};

IMPLEMENT_MODULE(FStudioModule, UE_MCP_Studio)

#undef LOCTEXT_NAMESPACE

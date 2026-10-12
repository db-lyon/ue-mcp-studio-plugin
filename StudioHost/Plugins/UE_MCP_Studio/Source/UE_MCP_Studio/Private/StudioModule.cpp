// Copyright 2026 ue-mcp. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Docking/TabManager.h"
#include "SStudioPanel.h"
#include "StudioDocs.h"
#include "StudioDaemon.h"
#include "StudioWindow.h"
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
			.SetDisplayName(LOCTEXT("TabTitle", "UE-MCP Studio Status"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Open Studio, its setup, and the state of the editor, the daemon and updates"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings"));

		FStudioDocs::Register();

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FStudioModule::RegisterMenus));

		OpenCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Studio.Open"),
			TEXT("Open UE-MCP Studio's window."),
			FConsoleCommandDelegate::CreateStatic(&FStudioModule::OpenStudio),
			ECVF_Default);

		// The daemon is the product; the window and the tab are only views. Start it with the editor.
		FStudioDaemon::EnsureRunning();
		// The window reads the editor's theme from disk, so it never has to ask the editor.
		FStudioWindow::PublishEditorTheme();
	}

	virtual void ShutdownModule() override
	{
		if (OpenCommand)
		{
			IConsoleManager::Get().UnregisterConsoleObject(OpenCommand);
			OpenCommand = nullptr;
		}
		FStudioDocs::Unregister();
		FStudioWindow::Shutdown();
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(StudioTab);
		}
	}

private:
	IConsoleObject* OpenCommand = nullptr;

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SStudioPanel)];
	}

	/** Studio's window; the status tab when Studio cannot run yet, whose setup page says why. */
	static void OpenStudio()
	{
		if (!FStudioWindow::Open())
		{
			FGlobalTabmanager::Get()->TryInvokeTab(StudioTab);
		}
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		FToolMenuSection& Section = Toolbar->FindOrAddSection("UEMCPStudio");
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"OpenUEMCPStudio",
			FUIAction(FExecuteAction::CreateStatic(&FStudioModule::OpenStudio)),
			LOCTEXT("ToolbarLabel", "Studio"),
			LOCTEXT("ToolbarTooltip", "Open UE-MCP Studio"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings")));

		UToolMenu* Tools = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& ToolsSection = Tools->FindOrAddSection("UEMCPStudio", LOCTEXT("ToolsSection", "UE-MCP"));
		ToolsSection.AddMenuEntry(
			"OpenUEMCPStudioWindow",
			LOCTEXT("MenuLabel", "UE-MCP Studio"),
			LOCTEXT("MenuTooltip", "Open UE-MCP Studio in its own window"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings"),
			FUIAction(FExecuteAction::CreateStatic(&FStudioModule::OpenStudio)));
	}
};

IMPLEMENT_MODULE(FStudioModule, UE_MCP_Studio)

#undef LOCTEXT_NAMESPACE

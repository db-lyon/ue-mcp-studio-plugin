// Copyright 2026 ue-mcp. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Docking/TabManager.h"
#include "StudioDocs.h"
#include "StudioDaemon.h"
#include "StudioDock.h"
#include "StudioQuickAsk.h"
#include "StudioWindow.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "UE_MCP_Studio"

DEFINE_LOG_CATEGORY_STATIC(LogStudioModule, Log, All);

class FStudioModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FStudioDocs::Register();
		// The "UE-MCP Studio" tab: Studio's window sits over it, and it shows the setup page until Studio can run.
		FStudioDock::Startup();
		// Ask Studio: the hotkey, Ctrl+Alt+right-click and "Chat about this" on actors and assets.
		FStudioQuickAsk::Startup();

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FStudioModule::RegisterMenus));

		OpenCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Studio.Open"),
			TEXT("Open UE-MCP Studio, in its editor tab or its own window."),
			FConsoleCommandDelegate::CreateStatic(&FStudioModule::OpenStudio),
			ECVF_Default);

		// The daemon is the product; the window and the tab are only views. Start it with the editor.
		FStudioDaemon::EnsureRunning();
		// The window reads the editor's theme from disk, so it never has to ask the editor.
		FStudioWindow::PublishEditorTheme();
		PublishPluginInfo();
	}

	virtual void ShutdownModule() override
	{
		if (OpenCommand)
		{
			IConsoleManager::Get().UnregisterConsoleObject(OpenCommand);
			OpenCommand = nullptr;
		}
		FStudioQuickAsk::Shutdown();
		FStudioDocs::Unregister();
		FStudioDock::Shutdown();
		FStudioWindow::Shutdown();
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	IConsoleObject* OpenCommand = nullptr;

	/**
	 * Saved/UE_MCP_Studio/plugin.json: this plugin's version, folder, and whether it is
	 * the Fab build, so Studio reads what the plugin is rather than guessing from its path.
	 */
	static void PublishPluginInfo()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE_MCP_Studio"));
		if (!Plugin.IsValid())
		{
			return;
		}
		TSharedPtr<FJsonObject> Descriptor = Plugin->GetDescriptor().CachedJson;
		if (!Descriptor.IsValid())
		{
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *Plugin->GetDescriptorFileName()))
			{
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Descriptor);
			}
		}
		FString FabUrl;
		if (Descriptor.IsValid())
		{
			Descriptor->TryGetStringField(TEXT("FabURL"), FabUrl);
		}
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("version"), Plugin->GetDescriptor().VersionName);
		Out->SetStringField(TEXT("dir"), FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()));
		Out->SetBoolField(TEXT("fab"), UE_MCP_STUDIO_FAB != 0);
		Out->SetStringField(TEXT("fabUrl"), FabUrl);
		FString Json;
		FJsonSerializer::Serialize(Out, TJsonWriterFactory<>::Create(&Json));
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UE_MCP_Studio"), TEXT("plugin.json")));
		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *File) && Existing == Json)
		{
			return;
		}
		if (!FFileHelper::SaveStringToFile(Json, *File))
		{
			UE_LOG(LogStudioModule, Warning, TEXT("could not write %s"), *File);
		}
	}

	/** Studio, docked or in its own window as the project keeps it; the UE-MCP Studio tab's setup page when Studio cannot run yet. */
	static void OpenStudio()
	{
		FStudioDock::OpenStudio();
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
			LOCTEXT("MenuTooltip", "Open UE-MCP Studio, in its editor tab or its own window"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings"),
			FUIAction(FExecuteAction::CreateStatic(&FStudioModule::OpenStudio)));
	}
};

IMPLEMENT_MODULE(FStudioModule, UE_MCP_Studio)

#undef LOCTEXT_NAMESPACE

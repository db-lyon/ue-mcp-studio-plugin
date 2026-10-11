// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StudioNativeBinding.generated.h"

/**
 * What the Studio page may ask the editor to do, bound into the page as
 * `window.ue.studio`. Unreal lowercases the names there and every call returns
 * a Promise. The set is closed on purpose: the page can reach nothing else.
 */
UCLASS()
class UStudioNativeBinding : public UObject
{
	GENERATED_BODY()

public:
	/** JSON: { node, bridgeInstalled, daemon: { port, version } | null, log }. */
	UFUNCTION()
	FString GetStatus();

	UFUNCTION()
	bool StartDaemon();

	/** Run `ue-mcp init --yes` for this project. The editor needs a restart afterwards. */
	UFUNCTION()
	bool InstallBridge();

	/** Start `ue-mcp init --dry-run`; GetSetupPlan returns its JSON once it has finished. */
	UFUNCTION()
	bool StartSetupPlan();

	/** The dry-run plan's JSON, or an empty string while it is still running. */
	UFUNCTION()
	FString GetSetupPlan();

	/**
	 * Run `ue-mcp init` with the page's choices, as JSON:
	 * { clients: string[], nativeTools: bool, context?: "micro"|"lean"|"full", feedback: bool, skills: bool }.
	 * Unknown clients or values are refused.
	 */
	UFUNCTION()
	bool RunSetup(const FString& OptionsJson);

	UFUNCTION()
	bool RestartEditor();

	/** Enable one of the plugins ue-mcp needs in the .uproject. Any other name is refused. */
	UFUNCTION()
	bool EnablePlugin(const FString& Name);

	UFUNCTION()
	bool OpenProjectFolder();

	/** JSON of the active editor theme's colors as #rrggbb, keyed by FStyleColors name, for the page's editor theme. */
	UFUNCTION()
	FString GetEditorTheme();

	/** Open the Node.js download page in the system browser. The only external address the page can open. */
	UFUNCTION()
	bool OpenNodeDownload();

	/**
	 * A turn finished while the user was elsewhere: show the message as an editor
	 * toast and, when the editor is not the active window, flash it in the taskbar.
	 */
	UFUNCTION()
	bool DrawAttention(const FString& Message);
};

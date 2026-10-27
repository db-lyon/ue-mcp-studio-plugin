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
	 * { clients: string[], disable: string[], nativeTools: bool, nativeExclude: string[],
	 *   context?: "micro"|"lean"|"full", feedback: bool, hook: bool, skills: bool }.
	 * Unknown clients, contexts or malformed category names are refused.
	 */
	UFUNCTION()
	bool RunSetup(const FString& OptionsJson);

	/** init's JSON result after RunSetup ({ ok, error?, steps, warnings }), or an empty string while it runs. */
	UFUNCTION()
	FString GetSetupResult();

	/** Start the GitHub sign-in feedback issues author with (`ue-mcp auth --json`). */
	UFUNCTION()
	bool StartFeedbackAuth();

	/** The sign-in's progress so far: the JSON lines `ue-mcp auth --json` has printed. */
	UFUNCTION()
	FString GetFeedbackAuth();

	/** The theme picked in Studio's settings for this project ("ue-mcp", "editor" or "editor-orange"), "ue-mcp" when none is. */
	UFUNCTION()
	FString GetStudioTheme();

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

	/** Open the Node.js download page in the system browser. */
	UFUNCTION()
	bool OpenNodeDownload();

	/** Open GitHub's device sign-in page in the system browser. With OpenNodeDownload, the only external addresses the page can open. */
	UFUNCTION()
	bool OpenGitHubSignIn();

	/**
	 * A turn finished while the user was elsewhere: show the message as an editor
	 * toast and, when the editor is not the active window, flash it in the taskbar.
	 */
	UFUNCTION()
	bool DrawAttention(const FString& Message);

	/** Open the ue-mcp documentation in its own editor tab, at a path under /docs (empty for the start page). */
	UFUNCTION()
	bool OpenDocs(const FString& Path);
};

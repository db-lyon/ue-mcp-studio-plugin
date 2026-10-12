// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Studio's own window. The editor paints its dock tabs on the game thread, so
 * a busy editor froze a Studio tab; the window is a separate process the
 * Studio extension starts (POST /v1/ext/studio/window on the project's
 * daemon), and it keeps working while the game thread is blocked. Where the
 * route or the window is missing, Studio opens in the system browser.
 */
class FStudioWindow
{
public:
	/**
	 * Open the window, or bring it forward. Starts the daemon first when it is not
	 * running and opens once it is up. Returns false when Studio cannot run yet
	 * (no Node.js, no bridge), which the status tab's setup page handles.
	 * Mode "docked" or "window" switches the window to it; null keeps its own.
	 */
	static bool Open(const TCHAR* Mode = nullptr);

	/** Whether this project's Studio window is open. */
	static bool IsOpen();

	/** The editor theme's colors as JSON, #rrggbb keyed by FStyleColors name. */
	static FString EditorThemeJson();

	/** Write the editor theme where the window reads it, when it changed. */
	static void PublishEditorTheme();

	static void Shutdown();
};

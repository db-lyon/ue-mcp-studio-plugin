// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Docked mode: Studio's window held over the "UE-MCP Studio" dock tab.
 *
 * The tab is only a placeholder. Studio stays a separate process with an
 * unowned window of its own, so a busy game thread never stalls it; it places
 * itself over the tab from the rect this side reports. Each Slate frame the
 * tab's content rect (physical pixels, relative to the client origin of the
 * top-level window that holds it), that window's HWND, and whether the tab is
 * on screen go into a named shared-memory block, written only when something
 * changed, with a named event set after each write. Studio writes its mode and
 * its requests (open or close this tab) into the same block.
 *
 * Names and layout are studio-window.exe's apps/window/src/dock.h, layout 1,
 * keyed by the daemon's project key. Change both together.
 *
 * The choice between docked and a window of its own is kept per project in
 * Saved/UE_MCP_Studio/window-mode.txt, which Studio's window reads and writes too.
 */
class FStudioDock
{
public:
	static const FName TabId;

	/** The status tab of plugins before 0.1.0-beta.31; saved layouts still name it. */
	static const FName LegacyStatusTabId;

	static void Startup();
	static void Shutdown();

	/** Open Studio the way the project keeps it: in the dock tab, or in its own window; the dock tab's setup page when Studio cannot run yet. */
	static bool OpenStudio();

	/** Open Studio once the setup page let it run (install consent, the bridge), in the project's mode; the open it stood in for. */
	static void OpenAfterSetup();

	/** Switch to docked or to a window of its own, keep the choice, and show Studio that way. */
	static void SetDocked(bool bDocked);

	/** The project keeps Studio docked. */
	static bool IsDockedMode();

	/** Studio's window is running and can dock (it writes its side of the block). */
	static bool StudioCanDock();

	/** Open the dock tab, or bring it to the front. False when the level editor is not up. */
	static bool OpenTab();

	/** Show the setup page in the dock tab, keeping the project's mode. */
	static bool OpenSetupPage();

	/** Close the dock tab when it is open. Studio hides; the mode is kept. */
	static void CloseTab();
};

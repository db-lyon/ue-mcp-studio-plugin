// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The ue-mcp documentation in its own editor tab. The tab stays on
 * https://ue-mcp.com; a link anywhere else opens in the system browser.
 */
class FStudioDocs
{
public:
	static void Register();
	static void Unregister();

	/** Open the tab at a path under /docs, or bring it forward and go there. Any other path is refused. */
	static bool Open(const FString& Path);
};

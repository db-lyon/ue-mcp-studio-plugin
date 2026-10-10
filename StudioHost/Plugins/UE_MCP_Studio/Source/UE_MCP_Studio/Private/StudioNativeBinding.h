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

	UFUNCTION()
	bool RestartEditor();

	/** Enable one of the plugins ue-mcp needs in the .uproject. Any other name is refused. */
	UFUNCTION()
	bool EnablePlugin(const FString& Name);

	UFUNCTION()
	bool OpenProjectFolder();

	/** Open the Node.js download page in the system browser. The only external address the page can open. */
	UFUNCTION()
	bool OpenNodeDownload();
};

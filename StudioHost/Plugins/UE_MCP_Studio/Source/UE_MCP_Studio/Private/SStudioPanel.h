// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "StudioDaemon.h"
#include "StudioNativeBinding.h"

class SWebBrowser;
class SWidgetSwitcher;

struct FWebNavigationRequest;

/**
 * Setup and status, shown in the UE-MCP Studio tab while Studio cannot run:
 * the install consent prompt until the user allows installs, then the
 * bundled setup page until the daemon is up (Node.js, the bridge, the
 * project's setup, starting the daemon), then the launcher with the state of
 * the editor, the daemon and updates.
 */
class SStudioPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStudioPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SStudioPanel() override;

private:
	bool Poll(float DeltaTime);
	/** True cancels: only the bundled pages may load in the setup view. */
	bool OnBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);
	bool OnBeforePopup(FString Url, FString Frame);

	FString BootstrapUrl() const;
	bool IsAllowed(const FString& Url) const;

	/** The daemon's view of the editor, and the update state, read over its HTTP API. */
	void RequestHealth(const FStudioDaemonInfo& Info);
	void RequestUpdates(const FStudioDaemonInfo& Info);

	TSharedRef<SWidget> MakeLauncher();
	/** What Studio installs and where, with Allow and Not now. */
	TSharedRef<SWidget> MakeConsent();

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SWebBrowser> Browser;
	TStrongObjectPtr<UStudioNativeBinding> Binding;
	FTSTicker::FDelegateHandle Ticker;
	bool bTriedLoadBridge = false;
	/** Port of the daemon the launcher shows, 0 while on the setup page. */
	int32 LivePort = 0;
	double LastHealth = 0.0;
	double LastUpdates = 0.0;

	FText DaemonText;
	FText EditorText;
	FText UpdateText;
};

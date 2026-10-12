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
 * The Studio status tab. Studio itself runs in its own window, apart from the
 * game thread; this tab launches it and shows the state of the editor, the
 * daemon and updates. Until the daemon is up it shows the bundled setup page
 * instead (Node.js, the bridge, the project's setup, starting the daemon).
 * The first time the daemon is up in an editor session that shows this tab,
 * the window opens by itself.
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

	/** The window opens by itself once per editor session. */
	static bool bAutoOpened;
};

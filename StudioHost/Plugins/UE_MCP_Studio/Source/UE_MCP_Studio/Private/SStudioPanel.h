#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "StudioNativeBinding.h"

class SWebBrowser;

struct FWebNavigationRequest;

/**
 * The dock tab's content: one browser view. It shows the bundled bootstrap page
 * until the project's daemon is up, then the UI the daemon serves, and follows
 * the daemon if it comes back on another port.
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
	/** True cancels: only the bundled pages and the daemon's own origin may load. */
	bool OnBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);
	bool OnBeforePopup(FString Url, FString Frame);

	FString BootstrapUrl() const;
	bool IsAllowed(const FString& Url) const;

	TSharedPtr<SWebBrowser> Browser;
	TStrongObjectPtr<UStudioNativeBinding> Binding;
	FTSTicker::FDelegateHandle Ticker;
	/** Port of the daemon the UI was loaded from, 0 while on the bootstrap page. */
	int32 LoadedPort = 0;
	bool bTriedLoadBridge = false;
	FString LoadedToken;
};

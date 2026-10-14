// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioDock.h"

#include "StudioDaemon.h"
#include "StudioWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "LevelEditor.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "SStudioPanel.h"
#include "Containers/Ticker.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "UE_MCP_Studio"

DEFINE_LOG_CATEGORY_STATIC(LogStudioDock, Log, All);

const FName FStudioDock::TabId(TEXT("UEMCPStudioDock"));
const FName FStudioDock::LegacyStatusTabId(TEXT("UEMCPStudio"));

/** The dock tab's content: a placeholder Studio's window covers. Its paint is how the plugin knows the tab is on screen. */
class SStudioDockArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStudioDockArea) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** The placeholder Studio covers while it can run, the setup and status panel while it cannot. */
	EActiveTimerReturnType Refresh(double, float);

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SBox> PanelSlot;
	bool bPanelMade = false;
};

namespace StudioDockProtocol
{
	// apps/window/src/dock.h in db-lyon/ue-mcp-studio, layout 1.
	constexpr uint32 Magic = 0x4B434453;
	constexpr uint32 Layout = 1;
	constexpr SIZE_T Size = 256;
	constexpr SIZE_T OffMagic = 0, OffLayout = 4;
	constexpr SIZE_T OffPluginSeq = 8, OffPluginPid = 12, OffHost = 16, OffX = 24, OffY = 28, OffW = 32, OffH = 36, OffFlags = 40, OffFrame = 44;
	constexpr SIZE_T OffStudioSeq = 64, OffStudioPid = 68, OffStudioMode = 72, OffRequestSeq = 76, OffRequest = 80;
	constexpr uint32 FlagOpen = 1, FlagVisible = 2, FlagOverlay = 4;
	constexpr uint32 ModeNone = 0, ModeDocked = 2;
	constexpr uint32 RequestOpenTab = 1, RequestCloseTab = 2;
}

// Named, not anonymous: the module builds as a unity blob, where anonymous namespaces of different files merge.
namespace StudioDockImpl
{
	using namespace StudioDockProtocol;

	struct FDockReport
	{
		uint64 Host = 0;
		int32 X = 0, Y = 0, W = 0, H = 0;
		uint32 Flags = 0;

		bool operator==(const FDockReport& O) const
		{
			return Host == O.Host && X == O.X && Y == O.Y && W == O.W && H == O.H && Flags == O.Flags;
		}
	};

	struct FStudioSide
	{
		uint32 Pid = 0;
		uint32 Mode = ModeNone;
		uint32 RequestSeq = 0;
		uint32 Request = 0;
	};

#if PLATFORM_WINDOWS
	HANDLE Mapping = nullptr;
	HANDLE Changed = nullptr;
#endif
	volatile uint8* Block = nullptr;

	FDockReport Last;
	bool bWrote = false;
	uint32 Frame = 0;
	bool bSeenStudio = false;
	uint32 SeenRequestSeq = 0;
	/** The tab opens only for the setup page; the kept mode stays as it is. */
	bool bSetupOnly = false;

	FDelegateHandle PostTickHandle;
	FDelegateHandle RenderedHandle;
	FDelegateHandle RegisterTabsHandle;
	FDelegateHandle LayoutHandle;

	TWeakPtr<SDockTab> Tab;
	TWeakPtr<SStudioDockArea> Area;
	TWeakPtr<SWindow> HostWindow;
	/** The top-level window the area was last painted into. */
	TWeakPtr<SWindow> PaintedWindow;
	/** The area's last painted rect, in Slate's desktop space: physical pixels. */
	FSlateRect PaintedRect;
	uint64 PaintCount = 0;
	uint64 PaintCountAtRender = 0;
	bool bPaintedLastRender = false;

	uint32 Load32(SIZE_T Off) { return *reinterpret_cast<volatile uint32*>(Block + Off); }
	void Store32(SIZE_T Off, uint32 V) { *reinterpret_cast<volatile uint32*>(Block + Off) = V; }
	void Store64(SIZE_T Off, uint64 V) { *reinterpret_cast<volatile uint64*>(Block + Off) = V; }

	FString ModeFile()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UE_MCP_Studio"), TEXT("window-mode.txt")));
	}

	FString CachedMode;
	double CachedModeAt = -1.0;

	/** The kept mode. Studio's window changes it too, so it is read again at most once a second. */
	FString SavedMode()
	{
		const double Now = FPlatformTime::Seconds();
		if (CachedModeAt < 0.0 || Now - CachedModeAt > 1.0)
		{
			FString Text;
			FFileHelper::LoadFileToString(Text, *ModeFile());
			CachedMode = Text.TrimStartAndEnd();
			CachedModeAt = Now;
		}
		return CachedMode;
	}

	void SaveMode(bool bDocked)
	{
		CachedMode = bDocked ? TEXT("docked") : TEXT("window");
		CachedModeAt = FPlatformTime::Seconds();
		FFileHelper::SaveStringToFile(CachedMode + TEXT("\n"), *ModeFile());
	}

	bool ReadStudio(FStudioSide& Out)
	{
		if (!Block || Load32(OffMagic) != Magic || Load32(OffLayout) != Layout)
		{
			return false;
		}
		for (int32 Attempt = 0; Attempt < 64; ++Attempt)
		{
			const uint32 Before = Load32(OffStudioSeq);
			if (Before & 1)
			{
				continue;
			}
			FStudioSide S;
			S.Pid = Load32(OffStudioPid);
			S.Mode = Load32(OffStudioMode);
			S.RequestSeq = Load32(OffRequestSeq);
			S.Request = Load32(OffRequest);
			if (Load32(OffStudioSeq) == Before)
			{
				Out = S;
				return true;
			}
		}
		return false;
	}

	void Write(const FDockReport& R)
	{
		const uint32 Seq = Load32(OffPluginSeq) | 1;
		Store32(OffPluginSeq, Seq);
		FPlatformMisc::MemoryBarrier();
		Store32(OffPluginPid, FPlatformProcess::GetCurrentProcessId());
		Store64(OffHost, R.Host);
		Store32(OffX, static_cast<uint32>(R.X));
		Store32(OffY, static_cast<uint32>(R.Y));
		Store32(OffW, static_cast<uint32>(R.W));
		Store32(OffH, static_cast<uint32>(R.H));
		Store32(OffFlags, R.Flags);
		Store32(OffFrame, ++Frame);
		FPlatformMisc::MemoryBarrier();
		Store32(OffPluginSeq, Seq + 1);
#if PLATFORM_WINDOWS
		if (Changed)
		{
			::SetEvent(Changed);
		}
#endif
		Last = R;
		bWrote = true;
	}

	bool IsAncestorOf(const SWindow* Candidate, TSharedPtr<SWindow> Window)
	{
		for (; Window.IsValid(); Window = Window->GetParentWindow())
		{
			if (Window.Get() == Candidate)
			{
				return true;
			}
		}
		return false;
	}

	/** Whether the window is above the host in the desktop's z-order; windows under the host cannot cover the tab. */
	bool AboveHost(const TSharedRef<SWindow>& W, const TSharedPtr<SWindow>& Host)
	{
#if PLATFORM_WINDOWS
		if (!W->GetNativeWindow().IsValid() || !Host->GetNativeWindow().IsValid())
		{
			return false;
		}
		const HWND Target = static_cast<HWND>(W->GetNativeWindow()->GetOSWindowHandle());
		for (HWND Above = ::GetWindow(static_cast<HWND>(Host->GetNativeWindow()->GetOSWindowHandle()), GW_HWNDPREV); Above; Above = ::GetWindow(Above, GW_HWNDPREV))
		{
			if (Above == Target)
			{
				return true;
			}
		}
		return false;
#else
		return true;
#endif
	}

	/** Any other editor window on screen over the rect: a menu, popup, tooltip, modal or drag decorator. */
	bool AnyWindowOver(const TArray<TSharedRef<SWindow>>& Windows, const TSharedPtr<SWindow>& Host, const FSlateRect& Rect)
	{
		for (const TSharedRef<SWindow>& W : Windows)
		{
			if (!IsAncestorOf(&W.Get(), Host) && W->IsVisible() && !W->IsWindowMinimized() && W->GetOpacity() > 0.f && AboveHost(W, Host))
			{
				const FSlateRect R = W->GetRectInScreen();
				if (R.GetSize().X > 0.f && R.GetSize().Y > 0.f && FSlateRect::DoRectanglesIntersect(R, Rect))
				{
					return true;
				}
			}
			if (AnyWindowOver(W->GetChildWindows(), Host, Rect))
			{
				return true;
			}
		}
		return false;
	}

	/** Where the tab is now, written only when it changed. */
	void Publish()
	{
		if (!Block)
		{
			return;
		}
		FDockReport R;
		const TSharedPtr<SDockTab> T = Tab.Pin();
		if (T.IsValid() && Area.IsValid())
		{
			R.Flags |= FlagOpen;
			const TSharedPtr<SWindow> Window = PaintedWindow.Pin();
			HostWindow = Window;
			if (Window.IsValid() && Window->GetNativeWindow().IsValid())
			{
#if PLATFORM_WINDOWS
				const HWND Hwnd = static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
				R.Host = static_cast<uint64>(reinterpret_cast<UPTRINT>(Hwnd));
				POINT Origin = {0, 0};
				::ClientToScreen(Hwnd, &Origin);
				const int32 Left = FMath::RoundToInt(PaintedRect.Left);
				const int32 Top = FMath::RoundToInt(PaintedRect.Top);
				R.X = Left - Origin.x;
				R.Y = Top - Origin.y;
				R.W = FMath::RoundToInt(PaintedRect.Right) - Left;
				R.H = FMath::RoundToInt(PaintedRect.Bottom) - Top;
#endif
				const bool bOnScreen = Window->IsVisible() && !Window->IsWindowMinimized() && T->IsForeground() && bPaintedLastRender;
				if (bOnScreen && R.W > 0 && R.H > 0)
				{
					R.Flags |= FlagVisible;
					if (FSlateApplication::Get().IsDragDropping() || AnyWindowOver(FSlateApplication::Get().GetTopLevelWindows(), Window, PaintedRect))
					{
						R.Flags |= FlagOverlay;
					}
				}
			}
		}
		if (!bWrote || !(R == Last))
		{
			Write(R);
		}
	}

	/** Studio's window asking for the tab: it opens it when docking, closes it when leaving. */
	void HandleStudioRequests()
	{
		FStudioSide S;
		if (!ReadStudio(S))
		{
			return;
		}
		if (!bSeenStudio || S.RequestSeq == SeenRequestSeq)
		{
			bSeenStudio = true;
			SeenRequestSeq = S.RequestSeq;
			return;
		}
		SeenRequestSeq = S.RequestSeq;
		if (S.Request == RequestOpenTab)
		{
			FStudioDock::OpenTab();
		}
		else if (S.Request == RequestCloseTab)
		{
			FStudioDock::CloseTab();
		}
	}

	/** Slate drew the window holding the tab since the last tick. */
	bool bHostDrewThisTick = false;

	void OnPostTick(float)
	{
		// Whether the area was in the host's draw says whether the tab is on screen. A tick
		// in which the host did not draw changes nothing: an idle editor draws nothing, and
		// under a modal Slate draws only the modal, so the tab keeps its last answer.
		if (bHostDrewThisTick)
		{
			bPaintedLastRender = PaintCount != PaintCountAtRender;
			PaintCountAtRender = PaintCount;
			bHostDrewThisTick = false;
		}
		Publish();
		HandleStudioRequests();
	}

	/** Right after Slate draws a window; during a live resize this is outside the tick, so the host's draw reports at once. */
	void OnWindowRendered(SWindow& Window)
	{
		// The area's paint names its window first, so a tab that moved counts its new window.
		if (PaintedWindow.Pin().Get() == &Window)
		{
			bHostDrewThisTick = true;
		}
		if (HostWindow.Pin().Get() == &Window)
		{
			Publish();
		}
	}

	void OnTabClosed(TSharedRef<SDockTab> Closed)
	{
		// A tab replaced by a newer one (the migration below) does not clear the newer one.
		if (Tab.Pin() != Closed)
		{
			return;
		}
		Tab.Reset();
		Area.Reset();
		PaintedWindow.Reset();
		bPaintedLastRender = false;
		Publish();
	}

	TSharedRef<SDockTab> MakeDockTab()
	{
		TSharedRef<SStudioDockArea> NewArea = SNew(SStudioDockArea);
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::PanelTab)
			.OnTabClosed_Static(&OnTabClosed)
			[
				NewArea
			];
		Tab = NewTab;
		Area = NewArea;
		bPaintedLastRender = false;
		UE_LOG(LogStudioDock, Log, TEXT("dock tab spawned"));
		// A layout holding this tab means docked, whatever opened it, but the setup page.
		if (!bSetupOnly)
		{
			SaveMode(true);
			FStudioWindow::Open(TEXT("docked"));
		}
		return NewTab;
	}

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		return MakeDockTab();
	}

	/**
	 * The status tab of plugins before 0.1.0-beta.31, still in saved layouts. The dock tab
	 * took its place: the layout extension puts a closed dock tab beside it, and on the next
	 * tick the dock tab opens there (docked) or Studio opens in its window, and this one closes.
	 */
	TSharedRef<SDockTab> SpawnLegacyStatusTab(const FSpawnTabArgs&)
	{
		TSharedRef<SDockTab> Legacy = SNew(SDockTab).TabRole(ETabRole::NomadTab);
		TWeakPtr<SDockTab> Weak = Legacy;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
		{
			const TSharedPtr<SDockTab> Old = Weak.Pin();
			FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
			const TSharedPtr<FTabManager> Tabs = LevelEditor ? LevelEditor->GetLevelEditorTabManager() : nullptr;
			if (!FStudioDock::IsDockedMode())
			{
				FStudioWindow::Open();
			}
			else if (Old.IsValid() && Tabs.IsValid())
			{
				// The dock tab goes where the status tab is. Older plugins could leave it in a
				// window of its own behind the editor; that copy closes, with its window.
				const TSharedPtr<SWindow> StatusWindow = FSlateApplication::Get().FindWidgetWindow(Old.ToSharedRef());
				const TSharedPtr<SDockTab> Dock = Tab.Pin();
				const bool bInPlace = Dock.IsValid() && FSlateApplication::Get().FindWidgetWindow(Dock.ToSharedRef()) == StatusWindow;
				if (!bInPlace)
				{
					if (Dock.IsValid())
					{
						Dock->RequestCloseTab();
					}
					Tabs->InsertNewDocumentTab(FStudioDock::LegacyStatusTabId, FStudioDock::TabId, FTabManager::FLiveTabSearch(), MakeDockTab());
				}
				UE_LOG(LogStudioDock, Log, TEXT("the dock tab took the old status tab's place (%s)"), bInPlace ? TEXT("already there") : TEXT("moved"));
			}
			if (Old.IsValid())
			{
				Old->RequestCloseTab();
			}
			UE_LOG(LogStudioDock, Log, TEXT("replaced the status tab of an older plugin in the saved layout"));
			return false;
		}));
		return Legacy;
	}

	void RegisterTabs(TSharedPtr<FTabManager> TabManager)
	{
		if (!TabManager.IsValid() || TabManager->HasTabSpawner(FStudioDock::TabId))
		{
			return;
		}
		TabManager->RegisterTabSpawner(FStudioDock::TabId, FOnSpawnTab::CreateStatic(&SpawnTab))
			.SetDisplayName(LOCTEXT("DockTabTitle", "UE-MCP Studio"))
			.SetTooltipText(LOCTEXT("DockTabTooltip", "UE-MCP Studio in a tab of the editor"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings"));
	}

	/** By default the tab opens beside Details, next to the viewport. */
	void RegisterLayout(FLayoutExtender& Extender)
	{
		Extender.ExtendLayout(LevelEditorTabIds::LevelEditorSelectionDetails, ELayoutExtensionPosition::After, FTabManager::FTab(FStudioDock::TabId, ETabState::ClosedTab));
		// Where an older plugin's status tab is in the layout, the dock tab takes its place.
		Extender.ExtendLayout(FTabId(FStudioDock::LegacyStatusTabId), ELayoutExtensionPosition::Before, FTabManager::FTab(FStudioDock::TabId, ETabState::ClosedTab));
	}

	TSharedPtr<FTabManager> LevelEditorTabs()
	{
		FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		return LevelEditor ? LevelEditor->GetLevelEditorTabManager() : nullptr;
	}

	bool StudioRunning(const FStudioSide& S)
	{
		return S.Pid != 0 && FPlatformProcess::IsApplicationRunning(S.Pid);
	}

	FText AreaText()
	{
		if (FStudioDock::StudioCanDock())
		{
			return LOCTEXT("DockShowing", "UE-MCP Studio");
		}
		if (FStudioWindow::IsOpen())
		{
			return LOCTEXT("DockOldStudio", "This version of Studio opens in its own window. Update Studio from its Settings > Update to dock it here.");
		}
		FStudioDaemonInfo Info;
		if (FStudioDaemon::ReadLive(Info))
		{
			return LOCTEXT("DockStarting", "Starting Studio.");
		}
		return LOCTEXT("DockNotRunning", "Starting Studio.");
	}
}

namespace DockImpl = StudioDockImpl;

void SStudioDockArea::Construct(const FArguments& InArgs)
{
	// Painted every frame, so the paint says the tab is on screen.
	ForceVolatile(true);
	ChildSlot
	[
		SAssignNew(Switcher, SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			// Studio's background, so a frame where the window is not over the tab yet looks the same.
			.BorderBackgroundColor(FLinearColor(FColor(0x0c, 0x0c, 0x0b)))
			.Padding(FMargin(20.f, 18.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Static(&DockImpl::AreaText)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 12.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Visibility_Lambda([]() { return !FStudioDock::StudioCanDock() && FStudioWindow::IsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
					.OnClicked_Lambda([]() { FStudioDock::SetDocked(false); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("DockOpenAsWindow", "Open as window"))
					]
				]
			]
		]
		+ SWidgetSwitcher::Slot()
		[
			SAssignNew(PanelSlot, SBox)
		]
	];
	Refresh(0.0, 0.f);
	RegisterActiveTimer(1.f, FWidgetActiveTimerDelegate::CreateSP(this, &SStudioDockArea::Refresh));
}

EActiveTimerReturnType SStudioDockArea::Refresh(double, float)
{
	// Until the daemon runs there is no Studio to show: the setup page and status take the tab.
	FStudioDaemonInfo Info;
	const bool bLive = FStudioDaemon::ReadLive(Info);
	if (!bLive && !bPanelMade)
	{
		// Made once it is needed: it holds a browser view.
		bPanelMade = true;
		PanelSlot->SetContent(SNew(SStudioPanel));
	}
	Switcher->SetActiveWidgetIndex(bLive ? 0 : 1);
	return EActiveTimerReturnType::Continue;
}

int32 SStudioDockArea::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Paint geometry is window-local; the tick-space geometry Slate set for this paint is on the desktop, in physical pixels.
	const FGeometry& Desktop = GetTickSpaceGeometry();
	DockImpl::PaintedRect = FSlateRect::FromPointAndExtent(FVector2D(Desktop.GetAbsolutePosition()), FVector2D(Desktop.GetAbsoluteSize()));
	++DockImpl::PaintCount;
	if (SWindow* Window = OutDrawElements.GetPaintWindow())
	{
		DockImpl::PaintedWindow = StaticCastSharedRef<SWindow>(Window->AsShared());
	}
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void FStudioDock::Startup()
{
#if PLATFORM_WINDOWS
	const FString Name = FString(TEXT("Local\\UEMCPStudioDock.")) + FStudioDaemon::ProjectKey();
	DockImpl::Mapping = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(DockImpl::Size), *Name);
	if (DockImpl::Mapping)
	{
		DockImpl::Block = static_cast<volatile uint8*>(::MapViewOfFile(DockImpl::Mapping, FILE_MAP_ALL_ACCESS, 0, 0, DockImpl::Size));
	}
	DockImpl::Changed = ::CreateEventW(nullptr, 0, 0, *(Name + TEXT(".changed")));
	if (DockImpl::Block && DockImpl::Load32(DockImpl::OffMagic) != DockImpl::Magic)
	{
		DockImpl::Store32(DockImpl::OffLayout, DockImpl::Layout);
		DockImpl::Store32(DockImpl::OffMagic, DockImpl::Magic);
	}
	if (!DockImpl::Block)
	{
		UE_LOG(LogStudioDock, Warning, TEXT("could not open the dock block %s (%u); Studio will not dock"), *Name, ::GetLastError());
	}
#endif
	// The first report says this editor has a dock tab, before any tab opens.
	DockImpl::Publish();

	if (FSlateApplication::IsInitialized())
	{
		DockImpl::PostTickHandle = FSlateApplication::Get().OnPostTick().AddStatic(&DockImpl::OnPostTick);
		if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
		{
#if UE_VERSION_OLDER_THAN(5, 8, 0)
			DockImpl::RenderedHandle = Renderer->OnSlateWindowRendered().AddLambda([](SWindow& Window, void*) { DockImpl::OnWindowRendered(Window); });
#else
			DockImpl::RenderedHandle = Renderer->OnSlateWindowRendered().AddStatic(&DockImpl::OnWindowRendered);
#endif
		}
	}

	// Saved layouts from older plugins hold the status tab; it is not in any menu now.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LegacyStatusTabId, FOnSpawnTab::CreateStatic(&DockImpl::SpawnLegacyStatusTab))
		.SetDisplayName(LOCTEXT("LegacyStatusTitle", "UE-MCP Studio"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	DockImpl::RegisterTabsHandle = LevelEditor.OnRegisterTabs().AddStatic(&DockImpl::RegisterTabs);
	DockImpl::LayoutHandle = LevelEditor.OnRegisterLayoutExtensions().AddStatic(&DockImpl::RegisterLayout);
	DockImpl::RegisterTabs(LevelEditor.GetLevelEditorTabManager());
}

void FStudioDock::Shutdown()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LegacyStatusTabId);
		FSlateApplication::Get().OnPostTick().Remove(DockImpl::PostTickHandle);
		if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
		{
			Renderer->OnSlateWindowRendered().Remove(DockImpl::RenderedHandle);
		}
	}
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		LevelEditor->OnRegisterTabs().Remove(DockImpl::RegisterTabsHandle);
		LevelEditor->OnRegisterLayoutExtensions().Remove(DockImpl::LayoutHandle);
		if (const TSharedPtr<FTabManager> Tabs = LevelEditor->GetLevelEditorTabManager())
		{
			Tabs->UnregisterTabSpawner(TabId);
		}
	}
	DockImpl::Tab.Reset();
	DockImpl::Area.Reset();
	if (DockImpl::Block)
	{
		// No tab any more: Studio hides, then follows the editor's exit.
		DockImpl::Write(DockImpl::FDockReport());
	}
#if PLATFORM_WINDOWS
	if (DockImpl::Block)
	{
		::UnmapViewOfFile(const_cast<uint8*>(DockImpl::Block));
	}
	if (DockImpl::Mapping)
	{
		::CloseHandle(DockImpl::Mapping);
	}
	if (DockImpl::Changed)
	{
		::CloseHandle(DockImpl::Changed);
	}
	DockImpl::Mapping = nullptr;
	DockImpl::Changed = nullptr;
#endif
	DockImpl::Block = nullptr;
}

bool FStudioDock::IsDockedMode()
{
	return DockImpl::SavedMode() == TEXT("docked");
}

bool FStudioDock::StudioCanDock()
{
	DockImpl::FStudioSide S;
	return DockImpl::ReadStudio(S) && S.Mode != DockImpl::ModeNone && DockImpl::StudioRunning(S);
}

bool FStudioDock::OpenTab()
{
	const TSharedPtr<FTabManager> Tabs = DockImpl::LevelEditorTabs();
	return Tabs.IsValid() && Tabs->TryInvokeTab(TabId).IsValid();
}

void FStudioDock::CloseTab()
{
	if (const TSharedPtr<SDockTab> T = DockImpl::Tab.Pin())
	{
		T->RequestCloseTab();
	}
}

bool FStudioDock::OpenStudio()
{
	if (IsDockedMode() && OpenTab())
	{
		FStudioWindow::Open(TEXT("docked"));
		return true;
	}
	if (FStudioWindow::Open(DockImpl::SavedMode() == TEXT("window") ? TEXT("window") : nullptr))
	{
		return true;
	}
	// Studio cannot run yet: the dock tab shows the setup page, and a project kept in a window stays so.
	TGuardValue<bool> SetupOnly(DockImpl::bSetupOnly, DockImpl::SavedMode() == TEXT("window"));
	return OpenTab();
}

void FStudioDock::OpenAfterSetup()
{
	if (IsDockedMode())
	{
		FStudioWindow::Open(TEXT("docked"));
	}
	else if (FStudioWindow::Open(TEXT("window")))
	{
		// The tab only held the setup page.
		CloseTab();
	}
}

void FStudioDock::SetDocked(bool bDocked)
{
	DockImpl::SaveMode(bDocked);
	if (bDocked)
	{
		OpenTab();
		FStudioWindow::Open(TEXT("docked"));
	}
	else
	{
		CloseTab();
		FStudioWindow::Open(TEXT("window"));
	}
}

#undef LOCTEXT_NAMESPACE

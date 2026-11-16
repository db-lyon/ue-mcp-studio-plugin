// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioDock.h"
#include "StudioDockCutouts.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/TextureRenderTarget2D.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SVirtualWindow.h"

using StudioDockCutouts::FCut;

// Named, not anonymous: the module builds as a unity blob.
namespace StudioDockCutoutsTestsImpl
{
	FString Text(const FSlateRect& R)
	{
		return FString::Printf(TEXT("(%g, %g, %g, %g)"), R.Left, R.Top, R.Right, R.Bottom);
	}
}
using StudioDockCutoutsTestsImpl::Text;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioDockCutoutsPackTest, "UE.Studio.Dock.Cutouts.Pack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioDockCutoutsPackTest::RunTest(const FString&)
{
	const FSlateRect Tab(1200.f, 100.f, 1600.f, 900.f);

	// The Content Browser drawer opened wide across the tab: clipped to it, tab-relative.
	TArray<FCut> Cuts = StudioDockCutouts::Pack({FSlateRect(10.f, 600.f, 1910.f, 1000.f)}, Tab, 10);
	TestEqual(TEXT("a drawer across the tab is one cutout"), Cuts.Num(), 1);
	TestTrue(TEXT("the drawer is clipped to the tab"), Cuts.Num() == 1 && Cuts[0] == FCut{0, 500, 400, 300});

	// A context menu over the tab's left edge, at fractional coordinates: rounded outward.
	Cuts = StudioDockCutouts::Pack({FSlateRect(1100.4f, 300.6f, 1250.2f, 420.5f)}, Tab, 10);
	TestTrue(TEXT("a menu over the edge keeps only its part over the tab, rounded outward"), Cuts.Num() == 1 && Cuts[0] == FCut{0, 200, 51, 121});

	// Beside the tab, or empty: nothing.
	Cuts = StudioDockCutouts::Pack({FSlateRect(0.f, 0.f, 100.f, 100.f), FSlateRect(1300.f, 300.f, 1300.f, 400.f)}, Tab, 10);
	TestEqual(TEXT("overlays beside the tab, and empty ones, cut nothing"), Cuts.Num(), 0);

	// More overlays than fit: the rest merge into the last.
	TArray<FSlateRect> Many;
	for (int32 I = 0; I < 12; ++I)
	{
		Many.Add(FSlateRect(1200.f + I * 10.f, 100.f, 1205.f + I * 10.f, 110.f));
	}
	Cuts = StudioDockCutouts::Pack(Many, Tab, 10);
	TestEqual(TEXT("at most the limit"), Cuts.Num(), 10);
	TestTrue(TEXT("the last one covers every overlay past the limit"), Cuts.Num() == 10 && Cuts[9] == FCut{90, 0, 25, 10});

	// A tab on a 150% monitor: Slate desktop space is physical pixels, as Studio's window is placed, so nothing scales.
	const FSlateRect Tab150(2880.f, 150.f, 3480.f, 1350.f);
	Cuts = StudioDockCutouts::Pack({FSlateRect(1935.f, 900.f, 4785.f, 1500.f)}, Tab150, 10);
	TestTrue(TEXT("at 150% the drawer cuts the same physical rect it covers"), Cuts.Num() == 1 && Cuts[0] == FCut{0, 750, 600, 450});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioDockCutoutsPaintedTest, "UE.Studio.Dock.Cutouts.Painted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioDockCutoutsPaintedTest::RunTest(const FString&)
{
	const FSlateRect Drawer(10.f, 400.f, 1910.f, 800.f);
	TestEqual(TEXT("an overlay painted where it is laid out"), Text(StudioDockCutouts::Painted(Drawer, FVector2D::ZeroVector, true)), Text(Drawer));
	TestEqual(TEXT("a drawer a quarter open shows its top quarter, slid to the bottom"), Text(StudioDockCutouts::Painted(Drawer, FVector2D(0.f, 300.f), true)), Text(FSlateRect(10.f, 700.f, 1910.f, 800.f)));
	TestTrue(TEXT("a drawer slid all the way out paints nothing"), StudioDockCutouts::IsEmpty(StudioDockCutouts::Painted(Drawer, FVector2D(0.f, 400.f), true)));
	TestEqual(TEXT("an overlay that does not clip paints where it slid"), Text(StudioDockCutouts::Painted(Drawer, FVector2D(0.f, 300.f), false)), Text(FSlateRect(10.f, 700.f, 1910.f, 1100.f)));
	TestTrue(TEXT("the window-sized popup layer is looked inside"), StudioDockCutouts::CoversWindow(FSlateRect(0.f, 0.f, 1920.f, 1080.f), FSlateRect(0.f, 0.f, 1920.f, 1080.f)));
	TestFalse(TEXT("a drawer is an overlay itself"), StudioDockCutouts::CoversWindow(Drawer, FSlateRect(0.f, 0.f, 1920.f, 1080.f)));
	return true;
}

/**
 * The overlay walk on a real window, painted offscreen: a dock area at the right,
 * a drawer across the bottom from the window's overlay layer, and a drawer that
 * is still sliding in.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioDockCutoutsWindowTest, "UE.Studio.Dock.Cutouts.InWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioDockCutoutsWindowTest::RunTest(const FString&)
{
	const FVector2D Size(1000.f, 800.f);
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(Size);
	TSharedPtr<SBox> Area;
	Window->SetContent(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
		+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(Area, SBox).WidthOverride(400.f)]);

	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, false);
	TestNotNull(TEXT("a render target"), Target);
	if (!Target)
	{
		return false;
	}
	Target->AddToRoot();
	FWidgetRenderer Renderer(false);
	TSharedRef<FHittestGrid> Grid = MakeShared<FHittestGrid>();
	auto Paint = [&]()
	{
		Renderer.DrawWindow(Target, *Grid, Window, 1.f, Size, 0.016f);
		FlushRenderingCommands();
	};

	TArray<FSlateRect> Over;
	Paint();
	FStudioDock::InWindowOverlays(Window, Area, Over);
	TestEqual(TEXT("with nothing open, the empty popup layer is not an overlay"), Over.Num(), 0);

	// A drawer across the bottom, as the Content Browser drawer opens from the window's overlay layer.
	TSharedRef<SBox> Drawer = SNew(SBox).HeightOverride(300.f);
	Window->AddOverlaySlot().VAlign(VAlign_Bottom).Padding(FMargin(10.f, 0.f, 10.f, 20.f))[Drawer];
	Paint();
	Over.Reset();
	FStudioDock::InWindowOverlays(Window, Area, Over);
	TestEqual(TEXT("the drawer is one overlay"), Over.Num(), 1);
	if (Over.Num() == 1)
	{
		TestEqual(TEXT("at its painted rect"), Text(Over[0]), Text(FSlateRect(10.f, 480.f, 990.f, 780.f)));
	}
	Window->RemoveOverlaySlot(Drawer);

	// A drawer a third of the way in: laid out at full height, its content slid down and clipped.
	TSharedRef<SBox> Sliding = SNew(SBox)
		.HeightOverride(300.f)
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			SNew(SBox).RenderTransform(FSlateRenderTransform(FVector2f(0.f, 200.f)))
		];
	Window->AddOverlaySlot().VAlign(VAlign_Bottom)[Sliding];
	Paint();
	Over.Reset();
	FStudioDock::InWindowOverlays(Window, Area, Over);
	TestEqual(TEXT("the sliding drawer is one overlay"), Over.Num(), 1);
	if (Over.Num() == 1)
	{
		TestEqual(TEXT("only the part slid in"), Text(Over[0]), Text(FSlateRect(0.f, 700.f, 1000.f, 800.f)));
	}
	Window->RemoveOverlaySlot(Sliding);

	Paint();
	Over.Reset();
	FStudioDock::InWindowOverlays(Window, Area, Over);
	TestEqual(TEXT("closed again, nothing"), Over.Num(), 0);

	Target->RemoveFromRoot();
	return true;
}

#endif

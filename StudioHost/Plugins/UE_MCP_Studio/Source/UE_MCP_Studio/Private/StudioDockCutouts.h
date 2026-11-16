// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

/**
 * The rect math behind the dock's cutouts: what the editor draws over the dock
 * tab, which Studio's window cuts out of itself so the editor shows through.
 * All rects are Slate desktop space, which is physical pixels, the space
 * Studio's window is placed in, so nothing here scales for DPI.
 *
 * Kept free of widgets so the automation tests can drive it.
 */
namespace StudioDockCutouts
{
	/** A cutout as the dock block holds it: relative to the tab's content origin, physical pixels. */
	struct FCut
	{
		int32 X = 0, Y = 0, W = 0, H = 0;

		bool operator==(const FCut& O) const { return X == O.X && Y == O.Y && W == O.W && H == O.H; }
	};

	inline bool IsEmpty(const FSlateRect& R)
	{
		return !(R.Right > R.Left && R.Bottom > R.Top);
	}

	/** The intersection, or an empty rect when they do not meet. */
	inline FSlateRect Intersect(const FSlateRect& A, const FSlateRect& B)
	{
		const FSlateRect R(FMath::Max(A.Left, B.Left), FMath::Max(A.Top, B.Top), FMath::Min(A.Right, B.Right), FMath::Min(A.Bottom, B.Bottom));
		return IsEmpty(R) ? FSlateRect(0.f, 0.f, 0.f, 0.f) : R;
	}

	inline FSlateRect Union(const FSlateRect& A, const FSlateRect& B)
	{
		if (IsEmpty(A))
		{
			return B;
		}
		if (IsEmpty(B))
		{
			return A;
		}
		return FSlateRect(FMath::Min(A.Left, B.Left), FMath::Min(A.Top, B.Top), FMath::Max(A.Right, B.Right), FMath::Max(A.Bottom, B.Bottom));
	}

	/** Whether the rect is the whole window, give or take a border: a layer to look inside, not an overlay itself. */
	inline bool CoversWindow(const FSlateRect& R, const FSlateRect& Window)
	{
		constexpr float Slack = 2.f;
		return R.Left <= Window.Left + Slack && R.Top <= Window.Top + Slack && R.Right >= Window.Right - Slack && R.Bottom >= Window.Bottom - Slack;
	}

	/**
	 * Where an overlay is painted. Its layout rect, moved by how far its content
	 * is painted from where it is laid out: a drawer lays its content out at its
	 * full height and slides it in with a render transform, so while it opens or
	 * closes only the slid part is on screen. An overlay that clips to its bounds
	 * shows only the part still inside them.
	 */
	inline FSlateRect Painted(const FSlateRect& Layout, const FVector2D& ContentOffset, bool bClips)
	{
		const FSlateRect Slid = Layout.OffsetBy(ContentOffset);
		return bClips ? Intersect(Slid, Layout) : Slid;
	}

	/**
	 * The cutouts for the tab: each rect clipped to the tab, taken relative to its
	 * origin and rounded outward, so a fractional edge never leaves a pixel of
	 * Studio over the editor. The tab's origin rounds as the reported tab rect
	 * does. At most Max: the rest merge into the last one.
	 */
	inline TArray<FCut> Pack(const TArray<FSlateRect>& Rects, const FSlateRect& Tab, int32 Max)
	{
		TArray<FCut> Out;
		const int32 OriginX = FMath::RoundToInt(Tab.Left);
		const int32 OriginY = FMath::RoundToInt(Tab.Top);
		FSlateRect Merged(0.f, 0.f, 0.f, 0.f);
		int32 Kept = 0;
		TArray<FSlateRect> Clipped;
		for (const FSlateRect& R : Rects)
		{
			const FSlateRect C = Intersect(R, Tab);
			if (!IsEmpty(C))
			{
				Clipped.Add(C);
			}
		}
		for (int32 I = 0; I < Clipped.Num(); ++I)
		{
			if (Max > 0 && Kept == Max - 1 && Clipped.Num() > Max)
			{
				Merged = Union(Merged, Clipped[I]);
				continue;
			}
			const int32 L = FMath::FloorToInt(Clipped[I].Left), T = FMath::FloorToInt(Clipped[I].Top);
			const int32 R = FMath::CeilToInt(Clipped[I].Right), B = FMath::CeilToInt(Clipped[I].Bottom);
			Out.Add({L - OriginX, T - OriginY, R - L, B - T});
			++Kept;
		}
		if (!IsEmpty(Merged))
		{
			const int32 L = FMath::FloorToInt(Merged.Left), T = FMath::FloorToInt(Merged.Top);
			const int32 R = FMath::CeilToInt(Merged.Right), B = FMath::CeilToInt(Merged.Bottom);
			Out.Add({L - OriginX, T - OriginY, R - L, B - T});
		}
		return Out;
	}
}

// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioDaemon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioDaemonPackageSpecTest, "UE.Studio.Daemon.PackageSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioDaemonPackageSpecTest::RunTest(const FString&)
{
	const FString Min = TEXT("1.4.3");
	const FString Latest = TEXT("ue-mcp@latest");
	TestEqual(TEXT("the bridge's own version"), FStudioDaemon::ChoosePackageSpec(TEXT("1.4.11-beta.8"), Min, Latest, TEXT("1.4.11-beta.8")), FString(TEXT("ue-mcp@1.4.11-beta.8")));
	TestEqual(TEXT("a bridge ahead of the extension keeps its own"), FStudioDaemon::ChoosePackageSpec(TEXT("1.4.11-beta.9"), Min, Latest, TEXT("1.4.11-beta.7")), FString(TEXT("ue-mcp@1.4.11-beta.9")));
	// The support case: a beta extension on a bridge moved back to the stable line.
	TestEqual(TEXT("an extension needing more than the bridge gets its minimum"), FStudioDaemon::ChoosePackageSpec(TEXT("1.4.10"), Min, Latest, TEXT("1.4.11-beta.7")), FString(TEXT("ue-mcp@1.4.11-beta.7")));
	TestEqual(TEXT("no bridge and a beta extension"), FStudioDaemon::ChoosePackageSpec(FString(), Min, Latest, TEXT("1.4.11-beta.7")), FString(TEXT("ue-mcp@1.4.11-beta.7")));
	TestEqual(TEXT("no bridge and no extension"), FStudioDaemon::ChoosePackageSpec(FString(), Min, Latest, FString()), Latest);
	TestEqual(TEXT("a bridge below the manifest minimum"), FStudioDaemon::ChoosePackageSpec(TEXT("1.4.2"), Min, Latest, FString()), Latest);
	TestEqual(TEXT("an extension minimum below the manifest's does not win"), FStudioDaemon::ChoosePackageSpec(FString(), Min, Latest, TEXT("1.4.0")), Latest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioDaemonChannelDowngradeTest, "UE.Studio.Daemon.ChannelDowngrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioDaemonChannelDowngradeTest::RunTest(const FString&)
{
	TestTrue(TEXT("a beta ahead of the stable channel is kept"), FStudioDaemon::ChannelWouldDowngrade(TEXT("0.1.24-beta.15"), TEXT("0.1.23")));
	TestFalse(TEXT("an older install takes the channel"), FStudioDaemon::ChannelWouldDowngrade(TEXT("0.1.22"), TEXT("0.1.23")));
	TestFalse(TEXT("the same version reinstalls"), FStudioDaemon::ChannelWouldDowngrade(TEXT("0.1.23"), TEXT("0.1.23")));
	TestFalse(TEXT("nothing installed"), FStudioDaemon::ChannelWouldDowngrade(FString(), TEXT("0.1.23")));
	TestFalse(TEXT("a stable install with no channel version"), FStudioDaemon::ChannelWouldDowngrade(TEXT("0.1.23"), FString()));
	TestTrue(TEXT("a release orders above its prerelease"), FStudioDaemon::CompareVersions(TEXT("0.1.24"), TEXT("0.1.24-beta.16")) > 0);
	TestTrue(TEXT("prerelease numbers order numerically"), FStudioDaemon::CompareVersions(TEXT("1.4.11-beta.10"), TEXT("1.4.11-beta.8")) > 0);
	return true;
}

#endif

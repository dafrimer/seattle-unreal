#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "GameFramework/GameModeBase.h"
#include "StreetGameMode.generated.h"

class AStreetCharacter;
class IImageWrapper;

struct FStreetView
{
    FString Id;
    FVector Position = FVector::ZeroVector;
    FVector Target = FVector::ZeroVector;
    float VerticalFov = 64.f;
    FString Provenance;
};

struct FStreetWallProbe
{
    FString Id;
    FVector Start = FVector::ZeroVector;
    FVector Direction = FVector::ZeroVector;
    double Distance = 0;
};

struct FStreetSceneSnapshot
{
    TWeakObjectPtr<AActor> Actor;
    FString Label;
    FTransform Transform;
    TArray<FString> ComponentStates;
};

UCLASS()
class SEATTLEUNREAL_API AStreetGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStreetGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

    void ResetWalking();
    void ToggleOverview();
    void ShowView(int32 Index);
    void ShowViewById(const FString& Id);
    void ReturnToWalking();
    AStreetCharacter* Walker() const;
    bool IsAutomatic() const { return bVerify || bRecord; }
    bool IsRecording() const { return bRecord; }
    bool IsFinished() const { return bFinished; }
    const FString& GetTitle() const { return Title; }
    const FString& GetDisclosure() const { return Disclosure; }
    const FString& GetModeTitle() const { return ModeTitle; }
    const FString& GetProvenance() const { return Provenance; }
    const FString& GetStatus() const { return Status; }
    const FString& GetCaptionTitle() const { return CaptionTitle; }
    const FString& GetCaption() const { return Caption; }
    const FString& GetCaptionMode() const { return CaptionMode; }
    double GetRecordTime() const { return RecordFrame / 24.0; }
    int32 GetViewCount() const { return Views.Num(); }

private:
    enum class EVerifyStage : uint8 { Outward, Return, WallSetup, WallSettle, WallDrive, ResetSettle, Views, Complete };
    bool LoadLayout(FString& Error);
    bool InitializeWalker();
    void BeginAutomatic();
    void TickVerify(float DeltaSeconds);
    void TickRecord(float DeltaSeconds);
    void SampleGround(float DeltaSeconds);
    void DriveToward(const FVector& Target, bool bEaseAtTarget);
    void RecordWaypoint(const FString& Leg, int32 Index);
    bool SetupWallProbe(int32 Index);
    void FinishWallProbe(int32 Index);
    void SetRecordChapter(int32 Index);
    void Capture(const FString& RelativeName, bool bRecordFrame);
    void OnScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels);
    void SnapshotWorld();
    bool CheckWorldInvariant();
    void Fail(const FString& Error);
    void Finish();
    TSharedRef<FJsonObject> BaseEvidence() const;
    bool SaveJson(const FString& Filename, const TSharedRef<FJsonObject>& Object) const;
    void AddError(const FString& Error);

    FString Title = TEXT("FIFTH / UNIVERSITY");
    FString Disclosure;
    FString ModeTitle = TEXT("STREET LEVEL");
    FString Provenance = TEXT("Walkable solid mesh reconstruction; approximate world coordinates");
    FString Status;
    FString CaptionTitle;
    FString Caption;
    FString CaptionMode;
    FString EvidenceDir;
    FString LayoutError;
    FVector SpawnFeet = FVector::ZeroVector;
    float SpawnYaw = 0.f;
    TArray<FVector> Route;
    TArray<FStreetView> Views;
    TArray<FStreetWallProbe> WallProbes;
    int32 ExpectedBuildings = 9;
    int32 ExpectedProps = 88;
    int32 ActualBuildings = 0;
    int32 ActualProps = 0;
    int32 ActualSurfaces = 0;
    TArray<FStreetSceneSnapshot> SceneSnapshot;
    TArray<FString> Errors;
    TArray<TSharedPtr<FJsonValue>> WaypointEvidence;
    TArray<TSharedPtr<FJsonValue>> WallEvidence;
    TArray<TSharedPtr<FJsonValue>> ViewEvidence;
    TArray<TSharedPtr<FJsonValue>> CaptureEvidence;
    TArray<TSharedPtr<FJsonValue>> ChapterEvidence;
    TArray<FString> VerifyViews;
    TArray<TSharedPtr<FJsonValue>> SetupEvidence;
    TSharedPtr<IImageWrapper> PngEncoder;
    FDelegateHandle ScreenshotHandle;
    TWeakObjectPtr<AStreetCharacter> CachedWalker;
    bool bVerify = false;
    bool bRecord = false;
    bool bInitialized = false;
    bool bReady = false;
    bool bFinished = false;
    bool bCapturePending = false;
    bool bPendingRecordFrame = false;
    bool bViewCaptureRequested = false;
    bool bPreviousFixedStep = false;
    bool bResetPassed = false;
    bool bWorldInvariantPassed = false;
    bool bWallSweepValid = false;
    bool bWasFalling = false;
    double PreviousFixedDelta = 0;
    double StartWallTime = 0;
    double SimulationSeconds = 0;
    double StageSeconds = 0;
    double WaypointSeconds = 0;
    double StableWarmupSeconds = 0;
    double WarmupSeconds = 0;
    double UnsupportedSeconds = 0;
    double LongestUnsupportedSeconds = 0;
    double GroundedSeconds = 0;
    double GroundSampleSeconds = 0;
    double MinFeetZ = TNumericLimits<double>::Max();
    double MaxFeetZ = -TNumericLimits<double>::Max();
    double DistanceWalked = 0;
    double ReturnError = 0;
    double ResetError = 0;
    double ResetYawError = 0;
    double ResetCameraError = 0;
    double WallSweepDistance = 0;
    double WallProjectedTravel = 0;
    double WallEndForwardSpeed = 0;
    FVector PreviousFeet = FVector::ZeroVector;
    FVector RouteStartFeet = FVector::ZeroVector;
    FVector WallStartFeet = FVector::ZeroVector;
    FVector InspectionWalkerFeet = FVector::ZeroVector;
    FName WallExpectedLabel;
    FString PendingCapturePath;
    FString PendingRelativeName;
    EVerifyStage VerifyStage = EVerifyStage::Outward;
    int32 RouteIndex = 1;
    int32 WallIndex = 0;
    int32 ViewIndex = 0;
    int32 RecordFrame = 0;
    int32 RecordChapter = -1;
    int32 CapturedFrames = 0;
    int32 ScreenshotWidth = 0;
    int32 ScreenshotHeight = 0;
    int32 InteractiveSafetyResets = 0;
    int32 FallingTransitions = 0;
};

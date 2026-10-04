#include "StreetGameMode.h"

#include "StreetCharacter.h"
#include "StreetHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogStreetRuntime, Log, All);

namespace
{
    constexpr int32 CaptureFps = 24;
    constexpr int32 CaptureFrames = 64 * CaptureFps;
    constexpr double WaypointTolerance = 25.0;

    TSharedRef<FJsonObject> VectorJson(const FVector& Vector)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("x"), Vector.X);
        Result->SetNumberField(TEXT("y"), Vector.Y);
        Result->SetNumberField(TEXT("z"), Vector.Z);
        return Result;
    }

    bool ReadVector(const FJsonObject& Object, const TCHAR* Name, FVector& Result)
    {
        const TSharedPtr<FJsonObject>* Value = nullptr;
        double X, Y, Z;
        if (!Object.TryGetObjectField(Name, Value) || !Value || !Value->IsValid()
            || !(*Value)->TryGetNumberField(TEXT("x"), X) || !(*Value)->TryGetNumberField(TEXT("y"), Y)
            || !(*Value)->TryGetNumberField(TEXT("z"), Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
        Result = FVector(X, Y, Z);
        return true;
    }

    bool ReadVectorValue(const TSharedPtr<FJsonValue>& Value, FVector& Result)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) return false;
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        double X, Y, Z;
        if (!Object->TryGetNumberField(TEXT("x"), X) || !Object->TryGetNumberField(TEXT("y"), Y)
            || !Object->TryGetNumberField(TEXT("z"), Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
        Result = FVector(X, Y, Z);
        return true;
    }

    FString SceneLabel(const AActor* Actor)
    {
        for (const FName& Tag : Actor->Tags)
        {
            const FString Label = Tag.ToString();
            if (Label.StartsWith(TEXT("building:")) || Label.StartsWith(TEXT("prop:")) || Label.StartsWith(TEXT("surface:"))) return Label;
        }
        return FString();
    }

    TArray<FString> ComponentStates(const AActor* Actor)
    {
        TInlineComponentArray<UStaticMeshComponent*> Components;
        Actor->GetComponents(Components);
        TArray<FString> States;
        for (const UStaticMeshComponent* Component : Components)
        {
            FString State = FString::Printf(TEXT("%s|%s|%d|%d|%s"), *Component->GetName(),
                *GetPathNameSafe(Component->GetStaticMesh()), static_cast<int32>(Component->GetCollisionEnabled()),
                static_cast<int32>(Component->GetCollisionResponseToChannel(ECC_Pawn)), *Component->GetComponentTransform().ToString());
            for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
                State += TEXT("|") + GetPathNameSafe(Component->GetMaterial(Index));
            States.Add(MoveTemp(State));
        }
        States.Sort();
        return States;
    }

    void SetCVar(const TCHAR* Name, int32 Value)
    {
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name)) Variable->Set(Value, ECVF_SetByCode);
    }
}

AStreetGameMode::AStreetGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    DefaultPawnClass = AStreetCharacter::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();
    HUDClass = AStreetHUD::StaticClass();
}

void AStreetGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    bVerify = FParse::Param(FCommandLine::Get(), TEXT("StreetVerify"));
    bRecord = FParse::Param(FCommandLine::Get(), TEXT("StreetRecord"));
    EvidenceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence"));
    FString Override;
    if (FParse::Value(FCommandLine::Get(), TEXT("StreetEvidence="), Override)) EvidenceDir = Override;
    EvidenceDir = FPaths::ConvertRelativePathToFull(EvidenceDir);
    if (!LoadLayout(LayoutError)) UE_LOG(LogStreetRuntime, Error, TEXT("STREET_LAYOUT_FAILED %s"), *LayoutError);
    if (bVerify && bRecord) LayoutError = TEXT("StreetVerify and StreetRecord are mutually exclusive.");
}

bool AStreetGameMode::LoadLayout(FString& Error)
{
    const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("StreetData/layout.json"));
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
    {
        Error = TEXT("Cannot read authoritative layout: ") + Path;
        return false;
    }
    double Schema = 0, Yaw = 0, Buildings = 0, Props = 0;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Root->TryGetNumberField(TEXT("schemaVersion"), Schema) || Schema != 1
        || !Root->TryGetStringField(TEXT("title"), Title) || !Root->TryGetStringField(TEXT("disclosure"), Disclosure)
        || !ReadVector(*Root, TEXT("spawn"), SpawnFeet) || !Root->TryGetNumberField(TEXT("spawnYaw"), Yaw)
        || !FMath::IsFinite(Yaw) || !Root->TryGetNumberField(TEXT("buildingCount"), Buildings)
        || !Root->TryGetNumberField(TEXT("propCount"), Props) || Buildings != 9 || Props != 88
        || !Root->TryGetArrayField(TEXT("walkRoute"), Values) || Values->Num() != 34)
    {
        Error = TEXT("Layout schema/spawn/counts/34-point route contract is invalid.");
        return false;
    }
    SpawnYaw = static_cast<float>(Yaw);
    ExpectedBuildings = static_cast<int32>(Buildings);
    ExpectedProps = static_cast<int32>(Props);
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FVector Point;
        if (!ReadVectorValue(Value, Point)) { Error = TEXT("Invalid route coordinate."); return false; }
        Route.Add(Point);
    }
    if (FVector::Dist(Route[0], SpawnFeet) > 0.1) { Error = TEXT("Route must begin at the spawn feet position."); return false; }
    if (!Root->TryGetArrayField(TEXT("views"), Values) || Values->Num() < 9) { Error = TEXT("Missing camera views."); return false; }
    TSet<FString> ViewIds;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) { Error = TEXT("Invalid view object."); return false; }
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FStreetView View;
        double Fov = 0;
        if (!Object->TryGetStringField(TEXT("id"), View.Id) || View.Id.IsEmpty() || ViewIds.Contains(View.Id)
            || !ReadVector(*Object, TEXT("position"), View.Position) || !ReadVector(*Object, TEXT("target"), View.Target)
            || View.Position.Equals(View.Target) || !Object->TryGetNumberField(TEXT("fov"), Fov)
            || !FMath::IsFinite(Fov) || Fov <= 0 || Fov >= 150 || !Object->TryGetStringField(TEXT("provenance"), View.Provenance))
        { Error = TEXT("Invalid camera pose/FOV/provenance."); return false; }
        View.VerticalFov = static_cast<float>(Fov);
        ViewIds.Add(View.Id);
        Views.Add(MoveTemp(View));
    }
    if (!ViewIds.Contains(TEXT("overview")) || !ViewIds.Contains(TEXT("hotel-corner"))) { Error = TEXT("Required authored camera missing."); return false; }
    if (!Root->TryGetArrayField(TEXT("wallProbes"), Values) || Values->Num() != 3) { Error = TEXT("Exactly three wall probes are required."); return false; }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) { Error = TEXT("Invalid wall probe object."); return false; }
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FStreetWallProbe Probe;
        if (!Object->TryGetStringField(TEXT("id"), Probe.Id) || !ReadVector(*Object, TEXT("start"), Probe.Start)
            || !ReadVector(*Object, TEXT("direction"), Probe.Direction) || FMath::Abs(Probe.Direction.Size() - 1.0) > 0.01
            || FMath::Abs(Probe.Direction.Z) > 0.001 || !Object->TryGetNumberField(TEXT("distance"), Probe.Distance)
            || !FMath::IsFinite(Probe.Distance) || Probe.Distance <= 0)
        { Error = TEXT("Invalid wall probe direction/distance."); return false; }
        Probe.Direction.Normalize();
        WallProbes.Add(MoveTemp(Probe));
    }
    return true;
}

void AStreetGameMode::BeginPlay()
{
    Super::BeginPlay();
    StartWallTime = FPlatformTime::Seconds();
    if (!IsAutomatic()) return;
    PreviousFixedDelta = FApp::GetFixedDeltaTime();
    bPreviousFixedStep = FApp::UseFixedTimeStep();
    FApp::SetFixedDeltaTime(bRecord ? 1.0 / CaptureFps : 1.0 / 60.0);
    FApp::SetUseFixedTimeStep(true);
    SetCVar(TEXT("r.VSync"), 0);
    SetCVar(TEXT("r.HDR.EnableHDROutput"), 0);
    SetCVar(TEXT("r.ScreenshotDelegate"), 1);
    ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &AStreetGameMode::OnScreenshot);
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    PngEncoder = Images.CreateImageWrapper(EImageFormat::PNG);
    if (!IFileManager::Get().MakeDirectory(*EvidenceDir, true)) { Fail(TEXT("Cannot create evidence directory: ") + EvidenceDir); return; }
    if (bRecord)
    {
        const FString FramesDir = FPaths::Combine(EvidenceDir, TEXT("walkthrough-frames"));
        if (IFileManager::Get().DirectoryExists(*FramesDir)) { Fail(TEXT("Preserve previous recording: move walkthrough-frames before recording again.")); return; }
        if (!IFileManager::Get().MakeDirectory(*FramesDir, true)) { Fail(TEXT("Cannot create walkthrough frame directory.")); return; }
        CaptureEvidence.Reserve(CaptureFrames);
    }
    if (!LayoutError.IsEmpty()) { Fail(LayoutError); return; }
    if (!FApp::CanEverRender() || !GDynamicRHI || FString(GDynamicRHI->GetName()).Contains(TEXT("Null")))
    { Fail(TEXT("Automated evidence requires a real rendered game viewport and GPU, not NullRHI.")); return; }
    UE_LOG(LogStreetRuntime, Display, TEXT("STREET_WARMUP mode=%s fixedDelta=%.9f"), bRecord ? TEXT("record") : TEXT("verify"), FApp::GetFixedDeltaTime());
}

void AStreetGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    if (IsAutomatic())
    {
        FApp::SetUseFixedTimeStep(bPreviousFixedStep);
        FApp::SetFixedDeltaTime(PreviousFixedDelta);
    }
    Super::EndPlay(EndPlayReason);
}

APawn* AStreetGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
    if (!LayoutError.IsEmpty()) return nullptr;
    return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer,
        FTransform(FRotator(0, SpawnYaw, 0), SpawnFeet + FVector(0, 0, 90)));
}

AStreetCharacter* AStreetGameMode::Walker() const
{
    return CachedWalker.IsValid() ? CachedWalker.Get() : Cast<AStreetCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

bool AStreetGameMode::InitializeWalker()
{
    AStreetCharacter* Character = Walker();
    if (!Character || !Character->GetController()) return false;
    CachedWalker = Character;
    Character->AddTickPrerequisiteActor(this);
    Character->GetCharacterMovement()->AddTickPrerequisiteActor(this);
    Character->SetAutomatic(IsAutomatic());
    Character->SetFeetPose(SpawnFeet, SpawnYaw);
    Character->SetCursorCaptured(!IsAutomatic());
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        PC->bEnableClickEvents = !IsAutomatic();
        PC->bEnableMouseOverEvents = !IsAutomatic();
        PC->SetViewTarget(Character);
    }
    PreviousFeet = Character->FeetLocation();
    bInitialized = true;
    return true;
}

void AStreetGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;
    if (!LayoutError.IsEmpty())
    {
        Status = LayoutError;
        if (IsAutomatic()) Fail(LayoutError);
        return;
    }
    if (!bInitialized && !InitializeWalker())
    {
        if (IsAutomatic() && FPlatformTime::Seconds() - StartWallTime > 30) Fail(TEXT("Native player/possessed Character did not become available."));
        return;
    }
    AStreetCharacter* Character = Walker();
    if (!Character) { if (IsAutomatic()) Fail(TEXT("Walker disappeared.")); return; }
    if (!IsAutomatic())
    {
        if (Character->FeetLocation().Z < FMath::Min(-2000.0, SpawnFeet.Z - 2500.0))
        {
            ++InteractiveSafetyResets;
            ResetWalking();
            Status = FString::Printf(TEXT("Fall recovery %d: restored spawn. R resets manually."), InteractiveSafetyResets);
        }
        return;
    }
    const double WallElapsed = FPlatformTime::Seconds() - StartWallTime;
    if (WallElapsed > (bRecord ? 5400.0 : 2400.0)) { Fail(TEXT("Automation real-time deadline exceeded.")); return; }
    if (bCapturePending) { Fail(TEXT("Viewport did not deliver the previous step's screenshot; refusing to skip a simulation frame.")); return; }
    if (!bReady)
    {
        Character->StopDriving();
        WarmupSeconds += DeltaSeconds;
        const bool bLoading = GetNumAsyncPackages() > 0 || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling());
        StableWarmupSeconds = bLoading ? 0 : StableWarmupSeconds + DeltaSeconds;
        Status = FString::Printf(TEXT("Warming actual viewport / shaders / exposure: %.1f s"), WarmupSeconds);
        if (WallElapsed > 1200.0) { Fail(TEXT("Shader/load/ground warmup timed out.")); return; }
        UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
        const bool bViewportReady = Viewport && Viewport->Viewport && Viewport->Viewport->GetSizeXY().X > 0 && Viewport->Viewport->GetSizeXY().Y > 0;
        if (WarmupSeconds >= 10.0 && StableWarmupSeconds >= 5.0 && bViewportReady)
        {
            if (!Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetCharacterMovement()->CurrentFloor.IsWalkableFloor())
            { Fail(TEXT("Spawn has no walkable CharacterMovement floor after warmup.")); return; }
            BeginAutomatic();
        }
        return;
    }
    const double ExpectedDelta = bRecord ? 1.0 / CaptureFps : 1.0 / 60.0;
    if (FMath::Abs(DeltaSeconds - ExpectedDelta) > 0.0001) { Fail(FString::Printf(TEXT("Fixed-step mismatch: actual %.9f expected %.9f."), DeltaSeconds, ExpectedDelta)); return; }
    SimulationSeconds += DeltaSeconds;
    if (Character->FeetLocation().Z < SpawnFeet.Z - 100.0) { Fail(TEXT("Walker fell below the street; no automatic evidence reset is permitted.")); return; }
    if (bVerify) TickVerify(DeltaSeconds);
    else TickRecord(DeltaSeconds);
}

void AStreetGameMode::BeginAutomatic()
{
    SnapshotWorld();
    if (!Errors.IsEmpty()) { Finish(); return; }
    bReady = true;
    RouteStartFeet = Walker()->FeetLocation();
    PreviousFeet = RouteStartFeet;
    if (FVector::Dist2D(RouteStartFeet, SpawnFeet) > 2.0) { Fail(TEXT("Walker moved horizontally during initial spawn/warmup.")); return; }
    RouteIndex = 1;
    VerifyViews.Add(TEXT("overview"));
    for (const FStreetView& View : Views) if (View.Id != TEXT("overview")) VerifyViews.Add(View.Id);
    RecordWaypoint(TEXT("outward"), 0);
    UE_LOG(LogStreetRuntime, Display, TEXT("STREET_READY mode=%s gpu=%s buildings=%d props=%d surfaces=%d"),
        bRecord ? TEXT("record") : TEXT("verify"), *GRHIAdapterName, ActualBuildings, ActualProps, ActualSurfaces);
}

void AStreetGameMode::SnapshotWorld()
{
    TSet<FString> RenderLabels;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        const FString Label = SceneLabel(*It);
        if (Label.IsEmpty()) continue;
        FStreetSceneSnapshot Snapshot;
        Snapshot.Actor = *It;
        Snapshot.Label = Label;
        Snapshot.Transform = It->GetActorTransform();
        Snapshot.ComponentStates = ComponentStates(*It);
        if (Snapshot.ComponentStates.IsEmpty()) AddError(TEXT("Tagged scene actor has no static mesh component: ") + Label);
        SceneSnapshot.Add(MoveTemp(Snapshot));
        if (It->ActorHasTag(TEXT("StreetCollisionOnly"))) continue;
        if (RenderLabels.Contains(Label)) AddError(TEXT("Duplicate render label: ") + Label);
        RenderLabels.Add(Label);
        if (Label.StartsWith(TEXT("building:"))) ++ActualBuildings;
        else if (Label.StartsWith(TEXT("prop:"))) ++ActualProps;
        else if (Label.StartsWith(TEXT("surface:"))) ++ActualSurfaces;
    }
    if (ActualBuildings != ExpectedBuildings || ActualProps != ExpectedProps || ActualSurfaces != 115)
        AddError(FString::Printf(TEXT("Scene counts mismatch: %d buildings / %d props / %d surfaces; expected 9/88/115."), ActualBuildings, ActualProps, ActualSurfaces));
}

bool AStreetGameMode::CheckWorldInvariant()
{
    bool bPassed = true;
    int32 TaggedActors = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (!SceneLabel(*It).IsEmpty()) ++TaggedActors;
    if (TaggedActors != SceneSnapshot.Num()) { AddError(TEXT("Tagged world actor count changed.")); bPassed = false; }
    for (const FStreetSceneSnapshot& Snapshot : SceneSnapshot)
    {
        const AActor* Actor = Snapshot.Actor.Get();
        if (!Actor || SceneLabel(Actor) != Snapshot.Label || !Actor->GetActorTransform().Equals(Snapshot.Transform, 0.001)
            || ComponentStates(Actor) != Snapshot.ComponentStates)
        {
            AddError(TEXT("World geometry/material/collision invariant changed: ") + Snapshot.Label);
            bPassed = false;
        }
    }
    return bPassed;
}

void AStreetGameMode::SampleGround(float DeltaSeconds)
{
    AStreetCharacter* Character = Walker();
    const FVector Feet = Character->FeetLocation();
    const bool bGrounded = Character->GetCharacterMovement()->IsMovingOnGround() && Character->GetCharacterMovement()->CurrentFloor.IsWalkableFloor();
    const bool bFalling = Character->GetCharacterMovement()->IsFalling();
    if (bFalling && !bWasFalling) ++FallingTransitions;
    bWasFalling = bFalling;
    GroundSampleSeconds += DeltaSeconds;
    if (bGrounded) { GroundedSeconds += DeltaSeconds; UnsupportedSeconds = 0; }
    else UnsupportedSeconds += DeltaSeconds;
    LongestUnsupportedSeconds = FMath::Max(LongestUnsupportedSeconds, UnsupportedSeconds);
    MinFeetZ = FMath::Min(MinFeetZ, Feet.Z);
    MaxFeetZ = FMath::Max(MaxFeetZ, Feet.Z);
    DistanceWalked += FVector::Dist(Feet, PreviousFeet);
    PreviousFeet = Feet;
    if (UnsupportedSeconds > 0.35) Fail(TEXT("Character lost walkable floor support for more than 0.35 seconds."));
}

void AStreetGameMode::DriveToward(const FVector& Target, bool bEaseAtTarget)
{
    FVector Delta = Target - Walker()->FeetLocation();
    Delta.Z = 0;
    const double Distance = Delta.Size();
    Walker()->FaceDirection(Delta);
    const double Input = bEaseAtTarget ? FMath::Clamp(Distance / 70.0, 0.15, 1.0) : 1.0;
    Walker()->DriveWorld(Delta.GetSafeNormal() * Input);
}

void AStreetGameMode::RecordWaypoint(const FString& Leg, int32 Index)
{
    const FVector Feet = Walker()->FeetLocation();
    TSharedRef<FJsonObject> Point = MakeShared<FJsonObject>();
    Point->SetStringField(TEXT("leg"), Leg);
    Point->SetNumberField(TEXT("index"), Index);
    Point->SetObjectField(TEXT("feet"), VectorJson(Feet));
    Point->SetObjectField(TEXT("targetFeet"), VectorJson(Route[Index]));
    Point->SetNumberField(TEXT("planarErrorCm"), FVector::Dist2D(Feet, Route[Index]));
    Point->SetNumberField(TEXT("simulationSeconds"), SimulationSeconds);
    Point->SetBoolField(TEXT("grounded"), Walker()->GetCharacterMovement()->IsMovingOnGround());
    WaypointEvidence.Add(MakeShared<FJsonValueObject>(Point));
}

void AStreetGameMode::TickVerify(float DeltaSeconds)
{
    AStreetCharacter* Character = Walker();
    StageSeconds += DeltaSeconds;
    if (SimulationSeconds > 300.0) { Fail(TEXT("Verification simulation deadline exceeded.")); return; }
    switch (VerifyStage)
    {
    case EVerifyStage::Outward:
    case EVerifyStage::Return:
    {
        SampleGround(DeltaSeconds);
        if (bFinished) return;
        WaypointSeconds += DeltaSeconds;
        Status = FString::Printf(TEXT("REAL CHARACTER WALK / %s / waypoint %d of 34"), VerifyStage == EVerifyStage::Outward ? TEXT("outward") : TEXT("return"), RouteIndex + 1);
        if (FVector::Dist2D(Character->FeetLocation(), Route[RouteIndex]) <= WaypointTolerance)
        {
            RecordWaypoint(VerifyStage == EVerifyStage::Outward ? TEXT("outward") : TEXT("return"), RouteIndex);
            WaypointSeconds = 0;
            if (VerifyStage == EVerifyStage::Outward && RouteIndex == Route.Num() - 1)
            {
                RecordWaypoint(TEXT("return"), RouteIndex);
                VerifyStage = EVerifyStage::Return;
                --RouteIndex;
                UE_LOG(LogStreetRuntime, Display, TEXT("STREET_VERIFY_OUTWARD_COMPLETE"));
            }
            else if (VerifyStage == EVerifyStage::Return && RouteIndex == 0)
            {
                Character->StopDriving();
                ReturnError = FVector::Dist2D(Character->FeetLocation(), RouteStartFeet);
                if (ReturnError > WaypointTolerance + 2.0) AddError(TEXT("Out/back return error exceeds 27 cm."));
                VerifyStage = EVerifyStage::WallSetup;
                StageSeconds = 0;
                UE_LOG(LogStreetRuntime, Display, TEXT("STREET_VERIFY_ROUTE_COMPLETE returnErrorCm=%.3f"), ReturnError);
                return;
            }
            else RouteIndex += VerifyStage == EVerifyStage::Outward ? 1 : -1;
        }
        if (WaypointSeconds > 8.0) { Fail(FString::Printf(TEXT("Route waypoint %d blocked/timed out; no teleport bypass."), RouteIndex)); return; }
        DriveToward(Route[RouteIndex], true);
        break;
    }
    case EVerifyStage::WallSetup:
        if (!SetupWallProbe(WallIndex)) { Finish(); return; }
        VerifyStage = EVerifyStage::WallSettle;
        StageSeconds = 0;
        break;
    case EVerifyStage::WallSettle:
        if (StageSeconds >= 0.75)
        {
            const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
            const FFindFloorResult& Floor = Move->CurrentFloor;
            UE_LOG(LogStreetRuntime, Display, TEXT("STREET_WALL_SETTLE id=%s mode=%s feetZ=%.3f velocityZ=%.3f floorBlocking=%d floorWalkable=%d floorDistance=%.3f floorActor=%s"),
                *WallProbes[WallIndex].Id, *Move->GetMovementName(), Character->FeetLocation().Z, Character->GetVelocity().Z,
                Floor.bBlockingHit, Floor.IsWalkableFloor(), Floor.FloorDist,
                Floor.HitResult.GetActor() ? *Floor.HitResult.GetActor()->GetActorNameOrLabel() : TEXT("none"));
            if (!Move->IsMovingOnGround()) { Fail(TEXT("Wall probe setup did not settle onto a walkable floor.")); return; }
            WallStartFeet = Character->FeetLocation();
            PreviousFeet = WallStartFeet;
            Character->ClearWallContacts();
            VerifyStage = EVerifyStage::WallDrive;
            StageSeconds = 0;
        }
        break;
    case EVerifyStage::WallDrive:
        SampleGround(DeltaSeconds);
        if (bFinished) return;
        Status = FString::Printf(TEXT("REAL CHARACTER WALL ATTEMPT / %s"), *WallProbes[WallIndex].Id);
        if (StageSeconds >= WallProbes[WallIndex].Distance / 420.0 + 1.0)
        {
            FinishWallProbe(WallIndex);
            Character->StopDriving();
            ++WallIndex;
            StageSeconds = 0;
            if (WallIndex < WallProbes.Num()) VerifyStage = EVerifyStage::WallSetup;
            else
            {
                ResetWalking();
                ResetError = FVector::Dist(Character->FeetLocation(), SpawnFeet);
                bResetPassed = ResetError < 0.01 && !Character->IsInspecting();
                if (!bResetPassed) AddError(TEXT("Reset failed to restore exact spawn feet and walking mode."));
                VerifyStage = EVerifyStage::ResetSettle;
            }
        }
        else Character->DriveWorld(WallProbes[WallIndex].Direction);
        break;
    case EVerifyStage::ResetSettle:
        if (StageSeconds >= 1.0)
        {
            bResetPassed = bResetPassed && Character->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist2D(Character->FeetLocation(), SpawnFeet) < 0.01;
            APlayerController* PC = Cast<APlayerController>(Character->GetController());
            if (!PC || !PC->PlayerCameraManager) { Fail(TEXT("Reset camera unavailable.")); return; }
            ResetYawError = FMath::Abs(FMath::FindDeltaAngleDegrees(static_cast<double>(SpawnYaw), PC->GetControlRotation().Yaw));
            ResetCameraError = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Character->FeetLocation() + FVector(0, 0, 165));
            bResetPassed = bResetPassed && ResetYawError < 0.01 && ResetCameraError < 0.1;
            if (!bResetPassed) AddError(TEXT("Reset spawn did not retain floor support, heading, or the 165 cm first-person eye."));
            InspectionWalkerFeet = Character->FeetLocation();
            ShowViewById(VerifyViews[0]);
            VerifyStage = EVerifyStage::Views;
            StageSeconds = 0;
        }
        break;
    case EVerifyStage::Views:
        Status = TEXT("AUTHORED CAMERA INSPECTION / walker stays in place / ") + VerifyViews[ViewIndex];
        if (FVector::Dist(Character->FeetLocation(), InspectionWalkerFeet) > 0.01) { Fail(TEXT("Inspection camera mutated the walker position.")); return; }
        if (!bViewCaptureRequested && StageSeconds >= 2.0)
        {
            const FStreetView* View = Views.FindByPredicate([this](const FStreetView& Item) { return Item.Id == VerifyViews[ViewIndex]; });
            APlayerController* PC = Cast<APlayerController>(Character->GetController());
            if (!View || !PC || !PC->PlayerCameraManager) { Fail(TEXT("Camera verification target is unavailable.")); return; }
            const double PositionError = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), View->Position);
            const double RotationError = (PC->PlayerCameraManager->GetCameraRotation() - (View->Target - View->Position).Rotation()).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator);
            int32 Width = 0, Height = 0;
            PC->GetViewportSize(Width, Height);
            const double ExpectedFov = FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(View->VerticalFov * 0.5)) * Width / FMath::Max(Height, 1)));
            const double FovError = FMath::Abs(PC->PlayerCameraManager->GetFOVAngle() - ExpectedFov);
            const bool bWorldStable = CheckWorldInvariant();
            const bool bPassed = PositionError < 0.1 && RotationError < 0.1 && FovError < 0.01 && bWorldStable;
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("id"), View->Id);
            Result->SetStringField(TEXT("provenance"), View->Provenance);
            Result->SetBoolField(TEXT("pass"), bPassed);
            Result->SetNumberField(TEXT("positionErrorCm"), PositionError);
            Result->SetNumberField(TEXT("rotationErrorDegrees"), RotationError);
            Result->SetNumberField(TEXT("verticalFovDegrees"), View->VerticalFov);
            Result->SetNumberField(TEXT("horizontalFovDegrees"), PC->PlayerCameraManager->GetFOVAngle());
            Result->SetNumberField(TEXT("fovErrorDegrees"), FovError);
            Result->SetBoolField(TEXT("walkerUnchanged"), true);
            Result->SetBoolField(TEXT("worldUnchanged"), bWorldStable);
            ViewEvidence.Add(MakeShared<FJsonValueObject>(Result));
            if (!bPassed) AddError(TEXT("Camera pose/FOV/world check failed: ") + View->Id);
            bViewCaptureRequested = true;
            Capture(TEXT("verify-") + View->Id + TEXT(".png"), false);
        }
        else if (bViewCaptureRequested && !bCapturePending)
        {
            ++ViewIndex;
            bViewCaptureRequested = false;
            StageSeconds = 0;
            if (ViewIndex < VerifyViews.Num()) ShowViewById(VerifyViews[ViewIndex]);
            else
            {
                ReturnToWalking();
                if (FVector::Dist(Character->FeetLocation(), InspectionWalkerFeet) > 0.01 || Character->IsInspecting()) AddError(TEXT("Return from inspection changed walker state."));
                bWorldInvariantPassed = CheckWorldInvariant();
                VerifyStage = EVerifyStage::Complete;
            }
        }
        break;
    case EVerifyStage::Complete:
        Finish();
        break;
    }
}

bool AStreetGameMode::SetupWallProbe(int32 Index)
{
    const FStreetWallProbe& Probe = WallProbes[Index];
    AStreetCharacter* Character = Walker();
    Character->SetFeetPose(Probe.Start, Probe.Direction.Rotation().Yaw);
    WallStartFeet = Character->FeetLocation();
    PreviousFeet = WallStartFeet;
    UnsupportedSeconds = 0;
    WallExpectedLabel = FName(*(TEXT("building:") + Probe.Id));
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(StreetWallProbe), false, Character);
    const FVector Start = Character->GetActorLocation();
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(28.f, 88.f);
    const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, Start + Probe.Direction * Probe.Distance,
        FQuat::Identity, ECC_Pawn, Capsule, Params);
    bWallSweepValid = bHit && !Hit.bStartPenetrating && Hit.GetActor() && Hit.GetActor()->ActorHasTag(WallExpectedLabel)
        && FMath::Abs(Hit.ImpactNormal.Z) < 0.65f && Hit.Distance > 5.0 && Hit.Distance < Probe.Distance - 10.0;
    WallSweepDistance = Hit.Distance;
    TSharedRef<FJsonObject> Setup = MakeShared<FJsonObject>();
    Setup->SetStringField(TEXT("reason"), TEXT("Explicit wall-probe setup cut; not route walking"));
    Setup->SetStringField(TEXT("probe"), Probe.Id);
    Setup->SetObjectField(TEXT("feet"), VectorJson(WallStartFeet));
    Setup->SetBoolField(TEXT("sweepPass"), bWallSweepValid);
    Setup->SetBoolField(TEXT("startPenetrating"), Hit.bStartPenetrating);
    Setup->SetStringField(TEXT("sweepHit"), Hit.GetActor() ? SceneLabel(Hit.GetActor()) : TEXT("none"));
    Setup->SetNumberField(TEXT("wallContactDistanceCm"), WallSweepDistance);
    SetupEvidence.Add(MakeShared<FJsonValueObject>(Setup));
    if (!bWallSweepValid) AddError(TEXT("Wall setup capsule sweep did not find the intended blocking facade: ") + Probe.Id);
    return bWallSweepValid;
}

void AStreetGameMode::FinishWallProbe(int32 Index)
{
    AStreetCharacter* Character = Walker();
    const FStreetWallProbe& Probe = WallProbes[Index];
    const FVector Delta = Character->FeetLocation() - WallStartFeet;
    WallProjectedTravel = FVector::DotProduct(Delta, Probe.Direction);
    WallEndForwardSpeed = FVector::DotProduct(Character->GetVelocity(), Probe.Direction);
    const double Lateral = (Delta - Probe.Direction * WallProjectedTravel).Size2D();
    const bool bExpectedContact = Character->WallContactLabels.Contains(WallExpectedLabel);
    const bool bPassed = bWallSweepValid && bExpectedContact && Character->WallContactCount > 0
        && WallProjectedTravel > 5.0 && WallProjectedTravel >= WallSweepDistance - 15.0 && WallProjectedTravel <= WallSweepDistance + 5.0
        && FMath::Abs(WallEndForwardSpeed) < 10.0 && Lateral < 40.0 && Character->GetCharacterMovement()->IsMovingOnGround();
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("id"), Probe.Id);
    Result->SetBoolField(TEXT("pass"), bPassed);
    Result->SetBoolField(TEXT("intendedBuildingContact"), bExpectedContact);
    Result->SetNumberField(TEXT("movementBlockingContacts"), Character->WallContactCount);
    Result->SetNumberField(TEXT("unobstructedAttemptDistanceCm"), Probe.Distance);
    Result->SetNumberField(TEXT("capsuleSweepContactDistanceCm"), WallSweepDistance);
    Result->SetNumberField(TEXT("projectedTravelCm"), WallProjectedTravel);
    Result->SetNumberField(TEXT("lateralTravelCm"), Lateral);
    Result->SetNumberField(TEXT("endForwardSpeedCmPerSecond"), WallEndForwardSpeed);
    Result->SetObjectField(TEXT("startFeet"), VectorJson(WallStartFeet));
    Result->SetObjectField(TEXT("endFeet"), VectorJson(Character->FeetLocation()));
    Result->SetStringField(TEXT("movementPath"), TEXT("AStreetCharacter::DriveWorld -> AddMovementInput -> UCharacterMovementComponent"));
    WallEvidence.Add(MakeShared<FJsonValueObject>(Result));
    if (!bPassed) AddError(TEXT("Real CharacterMovement wall attempt failed: ") + Probe.Id);
    UE_LOG(LogStreetRuntime, Display, TEXT("STREET_WALL_RESULT id=%s pass=%d travelCm=%.3f contactDistanceCm=%.3f contacts=%d"),
        *Probe.Id, bPassed, WallProjectedTravel, WallSweepDistance, Character->WallContactCount);
}

void AStreetGameMode::SetRecordChapter(int32 Index)
{
    static const double Starts[] = { 0, 12, 22, 32, 42, 53, 59 };
    static const double Ends[] = { 12, 22, 32, 42, 53, 59, 64 };
    static const TCHAR* Titles[] = {
        TEXT("FROM SPLATS TO A SOLID STREET"), TEXT("ARCHITECTURE WITH DEPTH"), TEXT("SURFACES AND STREETSCAPE"),
        TEXT("LANDMARK-SPECIFIC ASSETS"), TEXT("REBUILT, REUSABLE LANDMARKS"), TEXT("SOLID WALLS, NOT BILLBOARDS"), TEXT("PERSISTENT SCENE / WORK IN PROGRESS") };
    static const TCHAR* Captions[] = {
        TEXT("Mapped 5th Avenue: continuous road, sidewalks and the same editable building kit, now in Unreal."),
        TEXT("Recessed windows, arched theatre frontage, cornices and a readable marquee. Architectural details are inferred."),
        TEXT("The same scanned PBR road and stone, branching trees, benches and lamps; Unreal Lumen lighting."),
        TEXT("Fairmont's stone facade and courtyard. Unseen architectural details are inferred, not surveyed."),
        TEXT("Rainier's flared pedestal and tapered neighbour; 9 buildings, 13 prop assets, native Unreal PBR materials."),
        TEXT("The same walking controller attempts forward motion; the camera trails 4.5 m behind the capsule so the facade stays visible."),
        TEXT("Reset returns to the same assets. Stylized reconstruction; not surveyed or photorealistic.") };
    static const TCHAR* Modes[] = {
        TEXT("ACTUAL CHARACTER MOVEMENT WALK"), TEXT("PLAYER CAMERA / STATIONARY LOOK"), TEXT("ACTUAL CHARACTER MOVEMENT WALK"),
        TEXT("AUTHORED INSPECTION CAMERA / NOT WALKING"), TEXT("AUTHORED AERIAL INSPECTION / NOT WALKING"),
        TEXT("COLLISION DEMONSTRATION / EXPLICIT SETUP CUT / TRAILING CAMERA"), TEXT("RESET + ACTUAL WALK / UNREAL PLAYER") };
    RecordChapter = Index;
    CaptionTitle = Titles[Index];
    Caption = Captions[Index];
    CaptionMode = Modes[Index];
    TSharedRef<FJsonObject> Chapter = MakeShared<FJsonObject>();
    Chapter->SetNumberField(TEXT("start"), Starts[Index]);
    Chapter->SetNumberField(TEXT("end"), Ends[Index]);
    Chapter->SetStringField(TEXT("title"), CaptionTitle);
    Chapter->SetStringField(TEXT("caption"), Caption);
    Chapter->SetStringField(TEXT("mode"), CaptionMode);
    ChapterEvidence.Add(MakeShared<FJsonValueObject>(Chapter));
    UE_LOG(LogStreetRuntime, Display, TEXT("STREET_RECORD_CHAPTER %d %s"), Index + 1, *CaptionTitle);
}

void AStreetGameMode::TickRecord(float DeltaSeconds)
{
    if (RecordFrame >= CaptureFrames)
    {
        bWorldInvariantPassed = CheckWorldInvariant();
        if (!bWorldInvariantPassed || WallEvidence.Num() != 1 || !bResetPassed || CapturedFrames != CaptureFrames) AddError(TEXT("Recording did not satisfy collision/reset/world/frame-count invariants."));
        Finish();
        return;
    }
    AStreetCharacter* Character = Walker();
    const double Time = RecordFrame / static_cast<double>(CaptureFps);
    const int32 Chapter = Time < 12 ? 0 : Time < 22 ? 1 : Time < 32 ? 2 : Time < 42 ? 3 : Time < 53 ? 4 : Time < 59 ? 5 : 6;
    // The preceding rendered step is sampled before this step's labelled setup changes.
    if (RecordFrame > 0 && (RecordChapter == 0 || RecordChapter == 1 || RecordChapter == 2 || RecordChapter == 5 || RecordChapter == 6))
    {
        SampleGround(DeltaSeconds);
        if (bFinished) return;
    }
    if (Chapter != RecordChapter)
    {
        if (Chapter == 6) FinishWallProbe(1);
        Character->StopDriving();
        if (Chapter == 5)
        {
            if (!SetupWallProbe(1)) { Finish(); return; }
            Character->SetTrailingCamera(450.f, 60.f);
        }
        if (Chapter == 6)
        {
            Character->SetTrailingCamera(0.f, 0.f);
            ResetWalking();
            ResetError = FVector::Dist(Character->FeetLocation(), SpawnFeet);
            bResetPassed = ResetError < 0.01 && !Character->IsInspecting();
            if (!bResetPassed) AddError(TEXT("Record reset did not restore spawn."));
            RouteIndex = 1;
            PreviousFeet = Character->FeetLocation();
            UnsupportedSeconds = 0;
        }
        if (Chapter == 3) InspectionWalkerFeet = Character->FeetLocation();
        SetRecordChapter(Chapter);
    }
    if (Chapter == 0 || Chapter == 2 || Chapter == 6)
    {
        if (FVector::Dist2D(Character->FeetLocation(), Route[RouteIndex]) < 25.0 && RouteIndex < Route.Num() - 1) ++RouteIndex;
        DriveToward(Route[RouteIndex], false);
    }
    else if (Chapter == 1)
    {
        Character->StopDriving();
        const double T = (Time - 12) / 10.0;
        const float Pitch = static_cast<float>(FMath::Lerp(9.0, 20.0, FMath::Sin(T * PI)));
        const float Yaw = static_cast<float>(FMath::Lerp(-78.0, -24.0, T));
        Character->FaceDirection(FRotator(0, Yaw, 0).Vector(), Pitch);
    }
    else if (Chapter == 3)
    {
        const double T = (Time - 32) / 10.0;
        Character->SetInspection(FMath::Lerp(FVector(5500, -800, 300), FVector(6100, -200, 600), T), FVector(9300, 4000, 1600), 64.f);
    }
    else if (Chapter == 4)
    {
        const double T = (Time - 42) / 11.0;
        Character->SetInspection(FMath::Lerp(FVector(-30000, -24000, 25500), FVector(-21000, -32000, 27000), T), FVector(2000, 1500, 10500), 62.f);
    }
    else
    {
        Character->FaceDirection(WallProbes[1].Direction, 12.f);
        Character->DriveWorld(WallProbes[1].Direction);
        WallProjectedTravel = FVector::DotProduct(Character->FeetLocation() - WallStartFeet, WallProbes[1].Direction);
        if (Character->WallContactCount > 0)
            Caption = FString::Printf(TEXT("Forward input continues. Real wall contact holds the capsule %.2f m from the labelled setup position."), WallProjectedTravel / 100.0);
    }
    if ((Chapter == 3 || Chapter == 4) && FVector::Dist(Character->FeetLocation(), InspectionWalkerFeet) > 0.01)
    { Fail(TEXT("Recording inspection camera moved the walker.")); return; }
    Capture(FString::Printf(TEXT("walkthrough-frames/frame-%06d.png"), RecordFrame), true);
}

void AStreetGameMode::Capture(const FString& RelativeName, bool bRecordFrame)
{
    if (bCapturePending || FScreenshotRequest::IsScreenshotRequested()) { Fail(TEXT("Overlapping screenshot requests would invalidate frame cadence.")); return; }
    PendingRelativeName = RelativeName;
    PendingCapturePath = FPaths::Combine(EvidenceDir, RelativeName);
    bPendingRecordFrame = bRecordFrame;
    bCapturePending = true;
    FScreenshotRequest::RequestScreenshot(PendingCapturePath, true, false, false, FIntRect(), true);
}

void AStreetGameMode::OnScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels)
{
    if (!bCapturePending || bFinished) return;
    bCapturePending = false;
    if (Width <= 0 || Height <= 0 || static_cast<int64>(Width) * Height != Pixels.Num() || !PngEncoder.IsValid())
    { Fail(TEXT("Invalid screenshot dimensions/pixel buffer/encoder.")); return; }
    if (ScreenshotWidth != 0 && (ScreenshotWidth != Width || ScreenshotHeight != Height))
    { Fail(TEXT("Viewport dimensions changed during evidence capture.")); return; }
    ScreenshotWidth = Width;
    ScreenshotHeight = Height;
    if (!PngEncoder->SetRaw(Pixels.GetData(), static_cast<int64>(Pixels.Num()) * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
    { Fail(TEXT("PNG encoder rejected the actual viewport frame.")); return; }
    const TArray64<uint8> Encoded = PngEncoder->GetCompressed();
    if (Encoded.IsEmpty() || !FFileHelper::SaveArrayToFile(Encoded, *PendingCapturePath))
    { Fail(TEXT("Failed to save viewport PNG: ") + PendingCapturePath); return; }
    TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("file"), PendingRelativeName);
    Frame->SetNumberField(TEXT("width"), Width);
    Frame->SetNumberField(TEXT("height"), Height);
    Frame->SetObjectField(TEXT("walkerFeet"), VectorJson(Walker()->FeetLocation()));
    Frame->SetBoolField(TEXT("inspectionCamera"), Walker()->IsInspecting());
    Frame->SetBoolField(TEXT("grounded"), Walker()->GetCharacterMovement()->IsMovingOnGround());
    if (APlayerController* PC = Cast<APlayerController>(Walker()->GetController()))
    {
        if (PC->PlayerCameraManager)
        {
            Frame->SetObjectField(TEXT("cameraPosition"), VectorJson(PC->PlayerCameraManager->GetCameraLocation()));
            const FRotator Rotation = PC->PlayerCameraManager->GetCameraRotation();
            Frame->SetObjectField(TEXT("cameraPitchYawRollDegrees"), VectorJson(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
            Frame->SetNumberField(TEXT("horizontalFovDegrees"), PC->PlayerCameraManager->GetFOVAngle());
        }
    }
    if (bPendingRecordFrame)
    {
        Frame->SetNumberField(TEXT("frame"), RecordFrame);
        Frame->SetNumberField(TEXT("timeSeconds"), RecordFrame / static_cast<double>(CaptureFps));
        Frame->SetNumberField(TEXT("chapter"), RecordChapter);
        Frame->SetStringField(TEXT("caption"), Caption);
        ++CapturedFrames;
        if (RecordFrame % CaptureFps == 0) UE_LOG(LogStreetRuntime, Display, TEXT("STREET_RECORD_FRAME %d / %d"), RecordFrame, CaptureFrames);
        ++RecordFrame;
    }
    CaptureEvidence.Add(MakeShared<FJsonValueObject>(Frame));
}

void AStreetGameMode::ResetWalking()
{
    if (AStreetCharacter* Character = Walker())
    {
        Character->SetFeetPose(SpawnFeet, SpawnYaw);
        ModeTitle = TEXT("STREET LEVEL");
        Provenance = TEXT("Walkable solid mesh reconstruction; approximate world coordinates");
        if (IsAutomatic())
        {
            TSharedRef<FJsonObject> Setup = MakeShared<FJsonObject>();
            Setup->SetStringField(TEXT("reason"), TEXT("Explicit reset control"));
            Setup->SetObjectField(TEXT("feet"), VectorJson(Character->FeetLocation()));
            Setup->SetNumberField(TEXT("simulationSeconds"), SimulationSeconds);
            SetupEvidence.Add(MakeShared<FJsonValueObject>(Setup));
        }
    }
}

void AStreetGameMode::ToggleOverview()
{
    if (Walker() && Walker()->IsInspecting()) ReturnToWalking();
    else ShowViewById(TEXT("overview"));
}

void AStreetGameMode::ShowView(int32 Index)
{
    if (!Views.IsValidIndex(Index) || !Walker()) return;
    const FStreetView& View = Views[Index];
    Walker()->SetInspection(View.Position, View.Target, View.VerticalFov);
    ModeTitle = View.Id.ToUpper();
    Provenance = View.Provenance;
}

void AStreetGameMode::ShowViewById(const FString& Id)
{
    const int32 Index = Views.IndexOfByPredicate([&Id](const FStreetView& View) { return View.Id == Id; });
    if (Index == INDEX_NONE) { if (IsAutomatic()) Fail(TEXT("Required viewpoint missing: ") + Id); return; }
    ShowView(Index);
}

void AStreetGameMode::ReturnToWalking()
{
    if (Walker()) Walker()->ReturnToWalking();
    ModeTitle = TEXT("STREET LEVEL");
    Provenance = TEXT("Walkable solid mesh reconstruction; approximate world coordinates");
}

void AStreetGameMode::AddError(const FString& Error)
{
    Errors.AddUnique(Error);
    UE_LOG(LogStreetRuntime, Error, TEXT("STREET_ERROR %s"), *Error);
}

void AStreetGameMode::Fail(const FString& Error)
{
    if (bFinished) return;
    AddError(Error);
    Finish();
}

TSharedRef<FJsonObject> AStreetGameMode::BaseEvidence() const
{
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("schemaVersion"), 1);
    Result->SetBoolField(TEXT("success"), Errors.IsEmpty());
    Result->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
    Result->SetStringField(TEXT("graphicsDevice"), GRHIAdapterName);
    Result->SetStringField(TEXT("rhi"), GDynamicRHI ? FString(GDynamicRHI->GetName()) : TEXT("none"));
    Result->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Result->SetStringField(TEXT("disclosure"), Disclosure);
    Result->SetStringField(TEXT("coordinateSystem"), TEXT("Unreal centimeters; route/spawn/probes are feet; capsule center = feet + 90 cm"));
    Result->SetNumberField(TEXT("fixedDeltaSeconds"), bRecord ? 1.0 / CaptureFps : 1.0 / 60.0);
    Result->SetNumberField(TEXT("simulationSeconds"), SimulationSeconds);
    Result->SetNumberField(TEXT("wallSeconds"), FPlatformTime::Seconds() - StartWallTime);
    Result->SetNumberField(TEXT("warmupSimulationSeconds"), WarmupSeconds);
    Result->SetNumberField(TEXT("renderWidth"), ScreenshotWidth);
    Result->SetNumberField(TEXT("renderHeight"), ScreenshotHeight);
    Result->SetBoolField(TEXT("hudIncluded"), !CaptureEvidence.IsEmpty());
    Result->SetNumberField(TEXT("fallingTransitions"), FallingTransitions);
    if (bVerify)
    {
        Result->SetNumberField(TEXT("resetYawErrorDegrees"), ResetYawError);
        Result->SetNumberField(TEXT("resetCameraErrorCm"), ResetCameraError);
    }
    Result->SetBoolField(TEXT("worldInvariantPassed"), bWorldInvariantPassed);
    Result->SetBoolField(TEXT("resetPassed"), bResetPassed);
    Result->SetNumberField(TEXT("resetErrorCm"), ResetError);
    Result->SetNumberField(TEXT("safetyResets"), InteractiveSafetyResets);
    Result->SetNumberField(TEXT("distanceWalkedCm"), DistanceWalked);
    Result->SetNumberField(TEXT("groundSampleSeconds"), GroundSampleSeconds);
    Result->SetNumberField(TEXT("groundedSeconds"), GroundedSeconds);
    Result->SetNumberField(TEXT("longestUnsupportedSeconds"), LongestUnsupportedSeconds);
    if (GroundSampleSeconds > 0)
    {
        Result->SetNumberField(TEXT("minimumFeetZCm"), MinFeetZ);
        Result->SetNumberField(TEXT("maximumFeetZCm"), MaxFeetZ);
    }
    TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
    Counts->SetNumberField(TEXT("buildings"), ActualBuildings);
    Counts->SetNumberField(TEXT("props"), ActualProps);
    Counts->SetNumberField(TEXT("surfaces"), ActualSurfaces);
    Counts->SetNumberField(TEXT("taggedActorsIncludingCollisionHelpers"), SceneSnapshot.Num());
    Result->SetObjectField(TEXT("counts"), Counts);
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for (const FString& Error : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(Error));
    Result->SetArrayField(TEXT("errors"), ErrorValues);
    Result->SetArrayField(TEXT("wallProbes"), WallEvidence);
    Result->SetArrayField(TEXT("explicitSetupCuts"), SetupEvidence);
    Result->SetArrayField(TEXT("screenshots"), CaptureEvidence);
    return Result;
}

bool AStreetGameMode::SaveJson(const FString& Filename, const TSharedRef<FJsonObject>& Object) const
{
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    return FJsonSerializer::Serialize(Object, Writer) && FFileHelper::SaveStringToFile(Text, *FPaths::Combine(EvidenceDir, Filename), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void AStreetGameMode::Finish()
{
    if (bFinished) return;
    if (Walker()) Walker()->StopDriving();
    if (bVerify && Errors.IsEmpty())
    {
        if (VerifyStage != EVerifyStage::Complete || WaypointEvidence.Num() != Route.Num() * 2 || WallEvidence.Num() != 3
            || ViewEvidence.Num() != VerifyViews.Num() || CaptureEvidence.Num() != VerifyViews.Num() || !bResetPassed || !bWorldInvariantPassed)
            AddError(TEXT("Verification ended without all out/back waypoints, three wall attempts, reset, views and rendered screenshots."));
    }
    if (bRecord && Errors.IsEmpty() && (CapturedFrames != CaptureFrames || ChapterEvidence.Num() != 7)) AddError(TEXT("Recording ended without 1536 actual rendered frames and all seven chapters."));
    bFinished = true;
    TSharedRef<FJsonObject> Result = BaseEvidence();
    FString Filename;
    if (bRecord)
    {
        Filename = TEXT("capture.json");
        Result->SetStringField(TEXT("method"), TEXT("Deterministic running Unreal player viewport + HUD PNG capture at fixed simulation cadence; not a real-time performance benchmark, montage, diffusion output, or source-image slideshow."));
        Result->SetNumberField(TEXT("frames"), CapturedFrames);
        Result->SetNumberField(TEXT("fps"), CaptureFps);
        Result->SetNumberField(TEXT("durationSeconds"), CapturedFrames / static_cast<double>(CaptureFps));
        Result->SetNumberField(TEXT("expectedFrames"), CaptureFrames);
        Result->SetStringField(TEXT("frameDirectory"), FPaths::Combine(EvidenceDir, TEXT("walkthrough-frames")));
        Result->SetStringField(TEXT("framePattern"), TEXT("frame-%06d.png"));
        Result->SetArrayField(TEXT("chapters"), ChapterEvidence);
    }
    else
    {
        Filename = TEXT("runtime-verification.json");
        Result->SetStringField(TEXT("method"), TEXT("Actual native ACharacter/CharacterMovement out-and-back route and blocking facade attempts; only explicit setup/reset teleports. Authored camera inspection leaves walker and world unchanged."));
        Result->SetNumberField(TEXT("routePointsEachWay"), Route.Num());
        Result->SetNumberField(TEXT("returnErrorCm"), ReturnError);
        Result->SetNumberField(TEXT("waypointToleranceCm"), WaypointTolerance);
        Result->SetArrayField(TEXT("routeWaypoints"), WaypointEvidence);
        Result->SetArrayField(TEXT("viewpoints"), ViewEvidence);
    }
    const bool bSaved = SaveJson(Filename, Result);
    const bool bPassed = Errors.IsEmpty() && bSaved;
    if (!bSaved) UE_LOG(LogStreetRuntime, Error, TEXT("STREET_EVIDENCE_WRITE_FAILED %s"), *FPaths::Combine(EvidenceDir, Filename));
    UE_LOG(LogStreetRuntime, Display, TEXT("%s success=%d evidence=%s"), bRecord ? TEXT("STREET_RECORD_COMPLETE") : TEXT("STREET_VERIFY_COMPLETE"), bPassed, *FPaths::Combine(EvidenceDir, Filename));
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("StreetRuntime"));
}

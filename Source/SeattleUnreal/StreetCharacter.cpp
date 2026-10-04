#include "StreetCharacter.h"

#include "StreetGameMode.h"
#include "StreetHUD.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

AStreetCharacter::AStreetCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(28.f, 90.f);
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    BaseEyeHeight = 75.f;
    Eye = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonEye"));
    Eye->SetupAttachment(GetCapsuleComponent());
    Eye->SetRelativeLocation(FVector(0, 0, 75));
    Eye->bUsePawnControlRotation = true;
    Eye->bConstrainAspectRatio = false;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = 420.f;
    Movement->MaxStepHeight = 30.f;
    Movement->SetWalkableFloorAngle(46.f);
    Movement->GravityScale = 2200.f / 980.f;
    Movement->JumpZVelocity = FMath::Sqrt(2.f * 2200.f * 105.f);
    Movement->MaxAcceleration = 12000.f;
    Movement->BrakingDecelerationWalking = 12000.f;
    Movement->AirControl = 0.35f;
    Movement->MaxSimulationTimeStep = 1.f / 60.f;
    Movement->MaxSimulationIterations = 8;
    Movement->bAlwaysCheckFloor = true;
    Movement->bOrientRotationToMovement = false;
}

void AStreetCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
    }
}

void AStreetCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindKey(EKeys::Escape, IE_Pressed, this, &AStreetCharacter::ReleaseCursor);
    Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AStreetCharacter::RecaptureCursor).bConsumeInput = false;
    Input->BindKey(EKeys::R, IE_Pressed, this, &AStreetCharacter::ResetWalk);
    Input->BindKey(EKeys::F, IE_Pressed, this, &AStreetCharacter::ToggleOverview);
    Input->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AStreetCharacter::JumpPressed);
    Input->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
    Input->BindKey(EKeys::One, IE_Pressed, this, &AStreetCharacter::Reference1);
    Input->BindKey(EKeys::Two, IE_Pressed, this, &AStreetCharacter::Reference2);
    Input->BindKey(EKeys::Three, IE_Pressed, this, &AStreetCharacter::Reference3);
    Input->BindKey(EKeys::Four, IE_Pressed, this, &AStreetCharacter::Reference4);
    Input->BindKey(EKeys::Five, IE_Pressed, this, &AStreetCharacter::Reference5);
    Input->BindKey(EKeys::Six, IE_Pressed, this, &AStreetCharacter::Reference6);
    Input->BindKey(EKeys::Seven, IE_Pressed, this, &AStreetCharacter::Reference7);
    Input->BindKey(EKeys::Eight, IE_Pressed, this, &AStreetCharacter::Reference8);
    Input->BindKey(EKeys::Nine, IE_Pressed, this, &AStreetCharacter::Reference9);
}

void AStreetCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || bAutomatic || bInspecting) return;
    if (!PC->IsInputKeyDown(EKeys::LeftMouseButton)) ClearUiInput();

    FRotator Look = PC->GetControlRotation();
    if (bCursorCaptured)
    {
        float MouseX = 0, MouseY = 0;
        PC->GetInputMouseDelta(MouseX, MouseY);
        Look.Yaw += MouseX * 1.8f;
        Look.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch) + MouseY * 1.8f, -82.f, 82.f);
    }
    else Look.Yaw += UiTurn * 90.f * DeltaSeconds;
    Look.Roll = 0;
    PC->SetControlRotation(Look);

    const auto Down = [PC](const FKey& Key, const FKey& Alternate) { return PC->IsInputKeyDown(Key) || PC->IsInputKeyDown(Alternate); };
    FVector2D Input = UiMovement;
    Input.Y += (Down(EKeys::W, EKeys::Up) ? 1.f : 0.f) - (Down(EKeys::S, EKeys::Down) ? 1.f : 0.f);
    Input.X += (Down(EKeys::D, EKeys::Right) ? 1.f : 0.f) - (Down(EKeys::A, EKeys::Left) ? 1.f : 0.f);
    if (Input.SizeSquared() > 1.f) Input.Normalize();
    const FRotationMatrix Basis(FRotator(0, Look.Yaw, 0));
    DriveWorld(Basis.GetUnitAxis(EAxis::X) * Input.Y + Basis.GetUnitAxis(EAxis::Y) * Input.X,
        bUiRun || PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
}

void AStreetCharacter::DriveWorld(const FVector& Direction, bool bRun, bool bJump)
{
    if (bInspecting) return;
    FVector Planar(Direction.X, Direction.Y, 0);
    Planar = Planar.GetClampedToMaxSize(1.0);
    GetCharacterMovement()->MaxWalkSpeed = bRun ? 740.f : 420.f;
    AddMovementInput(Planar, 1.f);
    if (bJump) Jump();
}

void AStreetCharacter::FaceDirection(const FVector& Direction, float Pitch)
{
    if (Controller && !Direction.IsNearlyZero())
        Controller->SetControlRotation(FRotator(Pitch, Direction.Rotation().Yaw, 0));
}

FVector AStreetCharacter::FeetLocation() const
{
    return GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void AStreetCharacter::SetFeetPose(const FVector& Feet, float Yaw)
{
    ReturnToWalking();
    StopDriving();
    SetActorLocationAndRotation(Feet + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
        FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller) Controller->SetControlRotation(FRotator(0, Yaw, 0));
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->bForceNextFloorCheck = true;
    ClearWallContacts();
}

void AStreetCharacter::StopDriving()
{
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    StopJumping();
    ClearUiInput();
}

void AStreetCharacter::SetInspection(const FVector& Position, const FVector& Target, float VerticalFov)
{
    if (!bInspecting)
    {
        bWalkingWasCaptured = bCursorCaptured;
        StopDriving();
        GetCharacterMovement()->DisableMovement();
    }
    bInspecting = true;
    InspectionPosition = Position;
    InspectionRotation = (Target - Position).Rotation();
    InspectionVerticalFov = VerticalFov;
    SetCursorCaptured(false);
}

void AStreetCharacter::ReturnToWalking()
{
    if (!bInspecting) return;
    bInspecting = false;
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->bForceNextFloorCheck = true;
    if (!bAutomatic) SetCursorCaptured(bWalkingWasCaptured);
}

void AStreetCharacter::SetTrailingCamera(float BackCm, float UpCm)
{
    Eye->SetRelativeLocation(FVector(-BackCm, 0, 75 + UpCm));
}

void AStreetCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    float VerticalFov = 64.f;
    if (bInspecting)
    {
        OutResult.Location = InspectionPosition;
        OutResult.Rotation = InspectionRotation;
        VerticalFov = InspectionVerticalFov;
    }
    int32 Width = 0, Height = 0;
    if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->GetViewportSize(Width, Height);
    const float Aspect = Height > 0 ? static_cast<float>(Width) / Height : 16.f / 9.f;
    OutResult.FOV = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFov * 0.5f)) * Aspect));
    OutResult.AspectRatio = Aspect;
    OutResult.bConstrainAspectRatio = false;
    OutResult.AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
    OutResult.PerspectiveNearClipPlane = 8.f;
}

void AStreetCharacter::MoveBlockedBy(const FHitResult& Impact)
{
    Super::MoveBlockedBy(Impact);
    if (Impact.bBlockingHit && FMath::Abs(Impact.ImpactNormal.Z) < 0.65f)
    {
        ++WallContactCount;
        if (const AActor* Actor = Impact.GetActor())
            for (const FName& Tag : Actor->Tags) WallContactLabels.Add(Tag);
    }
}

void AStreetCharacter::ClearWallContacts()
{
    WallContactCount = 0;
    WallContactLabels.Reset();
}

void AStreetCharacter::ClearUiInput()
{
    UiMovement = FVector2D::ZeroVector;
    UiTurn = 0;
    bUiRun = false;
}

void AStreetCharacter::SetCursorCaptured(bool bCaptured)
{
    bCursorCaptured = bCaptured && !bAutomatic && !bInspecting;
    ClearUiInput();
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;
    PC->bShowMouseCursor = !bCursorCaptured && !bAutomatic;
    if (bCursorCaptured || bAutomatic)
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(Mode);
    }
    else
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
    }
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        Viewport->SetMouseCaptureMode(bCursorCaptured ? EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown : EMouseCaptureMode::NoCapture);
}

void AStreetCharacter::ResetWalk()
{
    if (!bAutomatic) if (AStreetGameMode* Mode = GetWorld()->GetAuthGameMode<AStreetGameMode>()) Mode->ResetWalking();
}

void AStreetCharacter::ToggleOverview()
{
    if (!bAutomatic) if (AStreetGameMode* Mode = GetWorld()->GetAuthGameMode<AStreetGameMode>()) Mode->ToggleOverview();
}

void AStreetCharacter::ReleaseCursor() { if (!bAutomatic) SetCursorCaptured(false); }

void AStreetCharacter::RecaptureCursor()
{
    if (bAutomatic || bInspecting || bCursorCaptured) return;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
        if (AStreetHUD* Hud = Cast<AStreetHUD>(PC->GetHUD()))
            if (Hud->IsPointerOverControls()) return;
    SetCursorCaptured(true);
}

void AStreetCharacter::JumpPressed() { if (!bAutomatic && !bInspecting) DriveWorld(FVector::ZeroVector, bUiRun, true); }
void AStreetCharacter::UiJump() { JumpPressed(); }
void AStreetCharacter::ShowReference(int32 Index)
{
    if (!bAutomatic) if (AStreetGameMode* Mode = GetWorld()->GetAuthGameMode<AStreetGameMode>()) Mode->ShowView(Index);
}
void AStreetCharacter::Reference1() { ShowReference(0); }
void AStreetCharacter::Reference2() { ShowReference(1); }
void AStreetCharacter::Reference3() { ShowReference(2); }
void AStreetCharacter::Reference4() { ShowReference(3); }
void AStreetCharacter::Reference5() { ShowReference(4); }
void AStreetCharacter::Reference6() { ShowReference(5); }
void AStreetCharacter::Reference7() { ShowReference(6); }
void AStreetCharacter::Reference8() { ShowReference(7); }
void AStreetCharacter::Reference9() { ShowReference(8); }

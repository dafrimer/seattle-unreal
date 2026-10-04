#include "StreetHUD.h"

#include "StreetCharacter.h"
#include "StreetGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const FLinearColor Panel(0.018f, 0.033f, 0.044f, 0.94f);
    const FLinearColor Ink(0.91f, 0.95f, 0.96f, 1.f);
    const FLinearColor Accent(0.47f, 0.85f, 0.79f, 1.f);
    const FLinearColor Muted(0.67f, 0.76f, 0.80f, 1.f);
}

void AStreetHUD::Label(const FString& Text, float X, float Y, float FontScale, FLinearColor Color)
{
    DrawText(Text, Color, X * Scale, Y * Scale, GEngine->GetMediumFont(), FontScale * Scale);
}

float AStreetHUD::Wrapped(const FString& Text, float X, float Y, float Width, float FontScale, FLinearColor Color)
{
    TArray<FString> Words;
    Text.ParseIntoArrayWS(Words);
    FString Line;
    float W = 0, H = 0;
    GetTextSize(TEXT("Ag"), W, H, GEngine->GetMediumFont(), FontScale * Scale);
    const float LineHeight = H / Scale + 4.f;
    for (const FString& Word : Words)
    {
        const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
        GetTextSize(Candidate, W, H, GEngine->GetMediumFont(), FontScale * Scale);
        if (!Line.IsEmpty() && W > Width * Scale)
        {
            Label(Line, X, Y, FontScale, Color);
            Y += LineHeight;
            Line = Word;
        }
        else Line = Candidate;
    }
    if (!Line.IsEmpty()) { Label(Line, X, Y, FontScale, Color); Y += LineHeight; }
    return Y;
}

void AStreetHUD::Button(const FString& Text, FName Id, float X, float Y, float Width)
{
    const FVector2D Position(X * Scale, Y * Scale);
    const FVector2D Size(Width * Scale, 30.f * Scale);
    DrawRect(FLinearColor(0.08f, 0.17f, 0.19f, 0.96f), Position.X, Position.Y, Size.X, Size.Y);
    Label(Text, X + 9.f, Y + 6.f, 1.0f, Ink);
    AddHitBox(Position, Size, Id, true);
    ControlRects.Emplace(Position, Position + Size);
}

void AStreetHUD::DrawHUD()
{
    Super::DrawHUD();
    ControlRects.Reset();
    if (!Canvas || !GEngine) return;
    const AStreetGameMode* Mode = GetWorld()->GetAuthGameMode<AStreetGameMode>();
    if (!Mode) return;
    Scale = FMath::Max(Canvas->ClipX / 1280.f, 0.1f);
    const float Height = Canvas->ClipY / Scale;
    const AStreetCharacter* Character = Mode->Walker();
    DrawRect(Panel, 0, 0, Canvas->ClipX, 48.f * Scale);
    Label(Mode->GetTitle() + TEXT("    |    RECONSTRUCTION PROGRESS    |    ACTUAL UNREAL PLAYER"), 25, 14, 1.0f, Accent);

    if (Mode->IsRecording())
    {
        const float Top = Height - 150.f;
        DrawRect(Panel, 0, Top * Scale, Canvas->ClipX, 150.f * Scale);
        Label(Mode->GetCaptionTitle().IsEmpty() ? TEXT("PREPARING ACTUAL GAMEPLAY CAPTURE") : Mode->GetCaptionTitle(), 25, Top + 10, 1.75f, Ink);
        Wrapped(Mode->GetCaption().IsEmpty() ? Mode->GetStatus() : Mode->GetCaption(), 25, Top + 43, 1225, 1.3f, Ink);
        Label(Mode->GetCaptionMode(), 25, Height - 40, 0.95f, Accent);
        Label(TEXT("FIXED-STEP 24 FPS CAPTURE / NOT A PERFORMANCE BENCHMARK"), 25, Height - 19, 0.8f, Muted);
        Label(FString::Printf(TEXT("UE 5.8   /   %04.1f s"), Mode->GetRecordTime()), 1040, Height - 40, 0.95f, Accent);
        return;
    }

    DrawRect(Panel, 16.f * Scale, 63.f * Scale, 720.f * Scale, 104.f * Scale);
    Label(Mode->GetModeTitle(), 29, 74, 1.4f, Ink);
    Wrapped(Mode->GetProvenance(), 29, 101, 687, 0.95f, Muted);
    if (!Mode->GetStatus().IsEmpty())
    {
        DrawRect(Panel, 16.f * Scale, 178.f * Scale, 950.f * Scale, 44.f * Scale);
        Label(Mode->GetStatus(), 29, 189, 1.0f, Accent);
    }
    const bool bControls = Character && !Character->IsCursorCaptured() && !Mode->IsAutomatic();
    const float PanelHeight = bControls ? 166.f : 88.f;
    const float Top = Height - PanelHeight;
    DrawRect(Panel, 0, Top * Scale, Canvas->ClipX, PanelHeight * Scale);
    Wrapped(Mode->GetDisclosure(), 25, Top + 9, 1220, 0.95f, Muted);
    if (Mode->IsAutomatic())
    {
        Label(TEXT("MEASURED GAMEPLAY VERIFICATION / actual CharacterMovement, scene collisions and viewport frames"), 25, Height - 35, 1.0f, Accent);
        return;
    }
    Label(TEXT("WASD / ARROWS move   MOUSE look   SHIFT run   SPACE jump   R reset   F overview/walk   1-9 views"), 25, Top + 38, 1.0f, Ink);
    if (!bControls)
    {
        Label(TEXT("ESC releases cursor for clickable controls. Click outside the panel to recapture."), 25, Top + 62, 0.95f, Accent);
        DrawRect(Accent, Canvas->ClipX * 0.5f - 2, Canvas->ClipY * 0.5f - 2, 4, 4);
        return;
    }
    const float Row = Top + 66;
    Button(TEXT("Reset [R]"), TEXT("reset"), 25, Row, 110);
    Button(TEXT("Overview [F]"), TEXT("overview"), 145, Row, 140);
    Button(TEXT("Walk / return"), TEXT("walk"), 295, Row, 145);
    for (int32 Index = 0; Index < FMath::Min(Mode->GetViewCount(), 9); ++Index)
        Button(FString::FromInt(Index + 1), FName(*FString::Printf(TEXT("ref:%d"), Index)), 456.f + Index * 48.f, Row, 38);
    const float MoveRow = Row + 40;
    Button(TEXT("Forward"), TEXT("forward"), 25, MoveRow, 105);
    Button(TEXT("Back"), TEXT("back"), 140, MoveRow, 80);
    Button(TEXT("Left"), TEXT("left"), 230, MoveRow, 80);
    Button(TEXT("Right"), TEXT("right"), 320, MoveRow, 80);
    Button(TEXT("Turn <"), TEXT("turn-left"), 410, MoveRow, 95);
    Button(TEXT("Turn >"), TEXT("turn-right"), 515, MoveRow, 95);
    Button(TEXT("Jump"), TEXT("jump"), 620, MoveRow, 85);
    Button(TEXT("Run"), TEXT("run"), 715, MoveRow, 80);
    Label(TEXT("Hold movement buttons; click the scene to capture the mouse."), 810, MoveRow + 7, 0.8f, Muted);
}

bool AStreetHUD::IsPointerOverControls() const
{
    float X = 0, Y = 0;
    if (!PlayerOwner || !PlayerOwner->GetMousePosition(X, Y)) return false;
    for (const FBox2D& Rect : ControlRects) if (Rect.IsInsideOrOn(FVector2D(X, Y))) return true;
    return false;
}

void AStreetHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    AStreetGameMode* Mode = GetWorld()->GetAuthGameMode<AStreetGameMode>();
    if (!Mode || Mode->IsAutomatic() || !Mode->Walker()) return;
    AStreetCharacter* Character = Mode->Walker();
    const FString Id = BoxName.ToString();
    if (Id == TEXT("reset")) Mode->ResetWalking();
    else if (Id == TEXT("overview")) Mode->ToggleOverview();
    else if (Id == TEXT("walk")) Mode->ReturnToWalking();
    else if (Id.StartsWith(TEXT("ref:"))) Mode->ShowView(FCString::Atoi(*Id.Mid(4)));
    else if (Id == TEXT("forward")) Character->SetUiMovement(FVector2D(0, 1));
    else if (Id == TEXT("back")) Character->SetUiMovement(FVector2D(0, -1));
    else if (Id == TEXT("left")) Character->SetUiMovement(FVector2D(-1, 0));
    else if (Id == TEXT("right")) Character->SetUiMovement(FVector2D(1, 0));
    else if (Id == TEXT("turn-left")) Character->SetUiTurn(-1);
    else if (Id == TEXT("turn-right")) Character->SetUiTurn(1);
    else if (Id == TEXT("jump")) Character->UiJump();
    else if (Id == TEXT("run")) { Character->SetUiRun(true); Character->SetUiMovement(FVector2D(0, 1)); }
}

void AStreetHUD::NotifyHitBoxRelease(FName BoxName)
{
    Super::NotifyHitBoxRelease(BoxName);
    if (AStreetGameMode* Mode = GetWorld()->GetAuthGameMode<AStreetGameMode>())
        if (Mode->Walker()) Mode->Walker()->ClearUiInput();
}

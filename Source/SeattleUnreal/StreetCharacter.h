#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StreetCharacter.generated.h"

class UCameraComponent;

UCLASS()
class SEATTLEUNREAL_API AStreetCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AStreetCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    virtual void MoveBlockedBy(const FHitResult& Impact) override;

    // Both human input and the evidence driver enter CharacterMovement here.
    void DriveWorld(const FVector& Direction, bool bRun = false, bool bJump = false);
    void FaceDirection(const FVector& Direction, float Pitch = 0.f);
    void SetFeetPose(const FVector& Feet, float Yaw);
    FVector FeetLocation() const;
    void SetInspection(const FVector& Position, const FVector& Target, float VerticalFov);
    void ReturnToWalking();
    // Moves the same attached camera behind the capsule; movement and collision are unchanged.
    void SetTrailingCamera(float BackCm, float UpCm);
    void StopDriving();
    void SetCursorCaptured(bool bCaptured);
    bool IsCursorCaptured() const { return bCursorCaptured; }
    bool IsInspecting() const { return bInspecting; }
    void SetAutomatic(bool bEnabled) { bAutomatic = bEnabled; }
    bool IsAutomatic() const { return bAutomatic; }
    void SetUiMovement(const FVector2D& Direction) { UiMovement = Direction; }
    void SetUiTurn(float Turn) { UiTurn = Turn; }
    void SetUiRun(bool bRun) { bUiRun = bRun; }
    void UiJump();
    void ClearUiInput();
    void ClearWallContacts();
    int32 WallContactCount = 0;
    TSet<FName> WallContactLabels;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Eye;

    bool bAutomatic = false;
    bool bInspecting = false;
    bool bCursorCaptured = false;
    bool bWalkingWasCaptured = true;
    bool bUiRun = false;
    FVector2D UiMovement = FVector2D::ZeroVector;
    float UiTurn = 0.f;
    FVector InspectionPosition = FVector::ZeroVector;
    FRotator InspectionRotation = FRotator::ZeroRotator;
    float InspectionVerticalFov = 64.f;
    void ResetWalk();
    void ToggleOverview();
    void ReleaseCursor();
    void RecaptureCursor();
    void JumpPressed();
    void ShowReference(int32 Index);
    void Reference1();
    void Reference2();
    void Reference3();
    void Reference4();
    void Reference5();
    void Reference6();
    void Reference7();
    void Reference8();
    void Reference9();
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StreetHUD.generated.h"

UCLASS()
class SEATTLEUNREAL_API AStreetHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxRelease(FName BoxName) override;
    bool IsPointerOverControls() const;

private:
    void Label(const FString& Text, float X, float Y, float FontScale, FLinearColor Color);
    float Wrapped(const FString& Text, float X, float Y, float Width, float FontScale, FLinearColor Color);
    void Button(const FString& Text, FName Id, float X, float Y, float Width);
    float Scale = 1.f;
    TArray<FBox2D> ControlRects;
};

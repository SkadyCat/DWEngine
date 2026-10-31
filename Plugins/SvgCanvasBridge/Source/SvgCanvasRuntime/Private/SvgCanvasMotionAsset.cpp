#include "SvgCanvasMotionAsset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    float Ease(float T, const FString& Name)
    {
        if (T == 0 || T == 1) return T;
        if (Name == TEXT("easeIn")) return T*T*T;
        if (Name == TEXT("easeOut")) return 1 - FMath::Pow(1 - T, 3);
        if (Name == TEXT("easeInOut")) return .5f - .5f * FMath::Cos(PI*T);
        if (Name == TEXT("backOut")) { const float P = T - 1; return 1 + 2.3f*P*P*P + 1.3f*P*P; }
        return T;
    }
    bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, float& Value)
    {
        double N; if (!Object->TryGetNumberField(Key, N) || !FMath::IsFinite(N)) return false;
        Value = N; return FMath::IsFinite(Value);
    }
}
FSvgMotionFrame FSvgMotionClip::Sample(float TimeSeconds, const FSvgMotionFrame& Start) const
{
    const float Progress = Loop ? FMath::Fmod(FMath::Max(0.f, TimeSeconds), Duration) / Duration : FMath::Clamp(TimeSeconds / Duration, 0.f, 1.f);
    int32 Index = 1; while (Index < Frames.Num() - 1 && Frames[Index].T < Progress) Index++;
    const FSvgMotionFrame& A = FromCurrent && Index == 1 ? Start : Frames[Index - 1]; const FSvgMotionFrame& B = Frames[Index];
    const float T = Ease(FMath::Clamp((Progress - Frames[Index - 1].T) / (B.T - Frames[Index - 1].T), 0.f, 1.f), Easing);
    FSvgMotionFrame Result;
    Result.X = FMath::Lerp(A.X, B.X, T); Result.Y = FMath::Lerp(A.Y, B.Y, T); Result.ScaleX = FMath::Lerp(A.ScaleX, B.ScaleX, T); Result.ScaleY = FMath::Lerp(A.ScaleY, B.ScaleY, T);
    Result.Rotation = FMath::Lerp(A.Rotation, B.Rotation, T); Result.Opacity = FMath::Lerp(A.Opacity, B.Opacity, T); return Result;
}
bool USvgCanvasMotionAsset::Decode(FString& Error)
{
    if (DecodedJson == DefinitionJson && !Clips.IsEmpty()) return true;
    Clips.Reset(); Bindings.Reset(); DecodedJson.Empty();
    TSharedPtr<FJsonObject> Object;
    auto Fail = [&]() { Error = TEXT("Invalid SVG motion asset: ") + SourceAssetId; Clips.Reset(); Bindings.Reset(); return false; };
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DefinitionJson), Object) || !Object) return Fail();
    double Version; FString Target;
    if (!Object->TryGetNumberField(TEXT("version"), Version) || Version != 1 || !Object->TryGetStringField(TEXT("target"), Target) || (Target != TEXT("visual") && Target != TEXT("content") && Target != TEXT("edge"))) return Fail();
    VisualOnly = Target == TEXT("visual");
    EdgeOnly = Target == TEXT("edge");
    if (EdgeOnly)
    {
        const TSharedPtr<FJsonObject>* Settings; FString Color;
        if (!Object->TryGetObjectField(TEXT("edge"), Settings) || !Number(*Settings, TEXT("width"), Edge.Width) || !Number(*Settings, TEXT("radius"), Edge.Radius) ||
            Edge.Width <= 0 || Edge.Width > 16 || Edge.Radius < 0 || Edge.Radius > 256 || !(*Settings)->TryGetStringField(TEXT("color"), Color) || Color.Len() != 7 || !Color.StartsWith(TEXT("#"))) return Fail();
        for (int32 I=1; I<Color.Len(); ++I) if (!FChar::IsHexDigit(Color[I])) return Fail();
        Edge.Color = FLinearColor::FromSRGBColor(FColor::FromHex(Color));
    }
    const TSharedPtr<FJsonObject>* ClipValues = nullptr; const TSharedPtr<FJsonObject>* BindingValues = nullptr;
    if (!Object->TryGetObjectField(TEXT("clips"), ClipValues) || !Object->TryGetObjectField(TEXT("bindings"), BindingValues) || (*ClipValues)->Values.IsEmpty() || (*ClipValues)->Values.Num() > 32) return Fail();
    for (const auto& Pair : (*ClipValues)->Values)
    {
        if (Pair.Value->Type != EJson::Object) return Fail(); const auto Value = Pair.Value->AsObject(); FSvgMotionClip Clip;
        if (!Number(Value, TEXT("duration"), Clip.Duration) || Clip.Duration < 1 || Clip.Duration > 600000 || !Value->TryGetBoolField(TEXT("loop"), Clip.Loop) || !Value->TryGetBoolField(TEXT("fromCurrent"), Clip.FromCurrent) || !Value->TryGetStringField(TEXT("easing"), Clip.Easing)) return Fail();
        if (Clip.Easing != TEXT("linear") && Clip.Easing != TEXT("easeIn") && Clip.Easing != TEXT("easeOut") && Clip.Easing != TEXT("easeInOut") && Clip.Easing != TEXT("backOut")) return Fail();
        Clip.Duration /= 1000;
        const TArray<TSharedPtr<FJsonValue>>* Frames = nullptr; if (!Value->TryGetArrayField(TEXT("keyframes"), Frames) || Frames->Num() < 2 || Frames->Num() > 64) return Fail();
        float Last = -1;
        for (const auto& Item : *Frames)
        {
            if (Item->Type != EJson::Object) return Fail(); const auto F = Item->AsObject(); FSvgMotionFrame Frame;
            if (!Number(F, TEXT("t"), Frame.T) || !Number(F, TEXT("x"), Frame.X) || !Number(F, TEXT("y"), Frame.Y) || !Number(F, TEXT("scaleX"), Frame.ScaleX) || !Number(F, TEXT("scaleY"), Frame.ScaleY) || !Number(F, TEXT("rotation"), Frame.Rotation) || !Number(F, TEXT("opacity"), Frame.Opacity)) return Fail();
            if (Frame.T <= Last || Frame.T < 0 || Frame.T > 1 || Frame.ScaleX <= 0 || Frame.ScaleY <= 0 || Frame.Opacity < 0 || Frame.Opacity > 1) return Fail();
            Last = Frame.T; Clip.Frames.Add(Frame);
        }
        if (Clip.Frames[0].T != 0 || Clip.Frames.Last().T != 1) return Fail();
        Clips.Add(FString(Pair.Key.ToView()), MoveTemp(Clip));
    }
    for (const auto& Pair : (*BindingValues)->Values)
    {
        FString Name; if (!Pair.Value->TryGetString(Name) || !Clips.Contains(Name)) return Fail(); Bindings.Add(FString(Pair.Key.ToView()), Name);
    }
    DecodedJson = DefinitionJson; return true;
}
const FSvgMotionClip* USvgCanvasMotionAsset::FindClip(const FString& Trigger, const FString& Previous) const
{
    const FString* Name = Bindings.Find(Previous + TEXT(">") + Trigger); if (!Name) Name = Bindings.Find(Trigger);
    return Name ? Clips.Find(*Name) : nullptr;
}

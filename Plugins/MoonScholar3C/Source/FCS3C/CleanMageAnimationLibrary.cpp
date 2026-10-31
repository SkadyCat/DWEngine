#include "CleanMageAnimationLibrary.h"
#include "Animation/AnimSequence.h"

ECleanSpellReleaseHand UCleanMageAnimationLibrary::GetSkillReleaseHand(FName SkillId) const
{
    const auto* Config = LoadObject<UCleanSkillAnimationSet>(nullptr,
        TEXT("/Game/Clean3C/Animation/Mage/DA_SkillAnimations.DA_SkillAnimations"));
    const auto* Hand = Config ? Config->SkillReleaseHands.Find(SkillId) : nullptr;
    return Hand && *Hand == ECleanSpellReleaseHand::LeftHand
        ? ECleanSpellReleaseHand::LeftHand : ECleanSpellReleaseHand::RightHand;
}

FName UCleanMageAnimationLibrary::GetSkillReleaseSocket(FName SkillId) const
{
    return GetSkillReleaseHand(SkillId) == ECleanSpellReleaseHand::LeftHand ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
}

bool UCleanMageAnimationLibrary::IsSpellAnimation(int32 Index) const
{
    return Animations.IsValidIndex(Index) && Animations[Index] &&
        Labels.IsValidIndex(Index) && !Labels[Index].Contains(TEXT("处决目标")) &&
        (SpellEligible.IsEmpty() || (SpellEligible.IsValidIndex(Index) && SpellEligible[Index]));
}

int32 UCleanMageAnimationLibrary::GetSkillAnimationIndex(FName SkillId) const
{
    const auto* Config = LoadObject<UCleanSkillAnimationSet>(nullptr,
        TEXT("/Game/Clean3C/Animation/Mage/DA_SkillAnimations.DA_SkillAnimations"));
    if (!Config) return INDEX_NONE;
    const auto* Sequence = Config->SkillAnimations.Find(SkillId);
    const int32 Index = Sequence ? Animations.IndexOfByKey(*Sequence) : INDEX_NONE;
    return IsSpellAnimation(Index) ? Index : INDEX_NONE;
}

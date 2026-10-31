// Keep the original merchant prefab; the interaction belongs to its actor.
class ADWOOutpostMerchant : ADWWebVFXActor
{
 UPROPERTY(DefaultComponent) UWidgetComponent Bubble;
 // A screen-space component keeps the authored card crisp and at its UI size.
 default Bubble.SetWidgetSpace(EWidgetSpace::Screen);
 default Bubble.SetCollisionProfileName(n"NoCollision");
 default Bubble.SetPivot(FVector2D(.5,1));
 default Bubble.SetDrawAtDesiredSize(false);
 default Bubble.SetRelativeLocation(FVector(0,0,340));
}

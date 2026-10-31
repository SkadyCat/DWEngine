#include "OpusHoverUI.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Input/Events.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool UOpusHoverUI::TestPointerButton(UWidget* Target,FKey Button,bool Pressed)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("DWOTransient"))||!Target||!FSlateApplication::IsInitialized())return false;
 auto Widget=Target->GetCachedWidget();if(!Widget.IsValid())return false;
 FWidgetPath Generated;auto& App=FSlateApplication::Get();
 if(!App.GeneratePathToWidgetUnchecked(Widget.ToSharedRef(),Generated)||!Generated.IsValid())return false;
 // A layout path has no virtual-pointer entries. Slate's pointer router requires
 // one optional entry per widget, even for a normal screen-space mouse.
 TArray<FWidgetAndPointer> PointerWidgets;
 for(int32 Index=0;Index<Generated.Widgets.Num();++Index)PointerWidgets.Emplace(Generated.Widgets[Index]);
 const FWidgetPath Path{MakeArrayView(PointerWidgets)};
 const FGeometry& Geometry=Target->GetCachedGeometry();const FVector2D Point=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5);
 TSet<FKey> Buttons;if(Pressed)Buttons.Add(Button);
 const FPointerEvent Event(FSlateApplication::CursorPointerIndex,Point,Point,Buttons,Button,0,FModifierKeysState());
 App.RoutePointerMoveEvent(Path,Event,false);
 // Offscreen windows receive no OS cursor movement between test frames. Keep
 // the injected mouse over its target on release, as a physical click would.
 if(!Widget->IsHovered())Widget->OnMouseEnter(Geometry,Event);
 const FReply Reply=Pressed?App.RoutePointerDownEvent(Path,Event):App.RoutePointerUpEvent(Path,Event);
 return Reply.IsEventHandled();
}

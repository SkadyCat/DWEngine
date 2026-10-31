// Presentation only. The rule engine keeps a single authoritative ownership set.
namespace DWOInventory
{
 TArray<FString> Slots(){TArray<FString> Out;Out.Add("weapon");Out.Add("offhand");Out.Add("helmet");Out.Add("chest");Out.Add("gloves");Out.Add("boots");Out.Add("belt");Out.Add("amulet");Out.Add("ring");return Out;}
 int StockCount(int Total,bool Equipped){return Math::Max(0,Total-(Equipped?1:0));}
 bool Wearable(FOpusEntry Source){return Source.Kind=="equipment"&&Slots().Contains(Source.Slot);}
 int Count(FOpusEntry Source,FString Compartment)
 {
  if(Compartment=="equipped")return Wearable(Source)&&Source.Equipped?1:0;
  if(Compartment=="stock"&&Wearable(Source))return StockCount(Source.Count,Source.Equipped);
  return Source.Count;
 }
 bool Worn(FOpusEntry Source,FString Compartment){return Source.Equipped&&Compartment!="stock";}
 FString Role(UWidget Widget)
 {
  UWidget Current=Widget;while(Current!=nullptr){auto Node=Cast<USvgCanvasNode>(Current);if(Node!=nullptr){if((Node.Binding=="equipped-bag-grid"||Node.Binding=="gear-list"))return "equipped";if(Node.Binding=="bag-grid"||Node.Binding=="character-grid")return "stock";}Current=Current.GetParent();}return "";
 }
}

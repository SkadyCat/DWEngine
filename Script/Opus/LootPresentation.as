// RuleStudio loot-style / loot-markers presentation. No reward RNG or rules.
namespace DWOLoot
{
 FString Family(FOpusEntry E)
 {
  if(E.Category=="currency")return E.CurrencyGrade>=1&&E.CurrencyGrade<=3?"currency-"+E.CurrencyGrade:"currency-special";
  if(E.Category=="omen")return "omen";
  return E.Rarity=="magic"||E.Rarity=="rare"||E.Rarity=="legendary"||E.Rarity=="unique"?E.Rarity:"common";
 }
 FString Name(FOpusEntry E){return (E.Tier>0?"T"+E.Tier+" · ":"")+E.Name+(!E.Identified?" · 未鉴定":"")+(E.Count>1?" ×"+E.Count:"");}
 FLinearColor Color(FOpusEntry E)
 {
  FString F=Family(E);
  if(F=="currency-1")return FLinearColor(0.847,0.788,0.627);
  if(F=="currency-2")return FLinearColor(0.506,0.886,0.827);
  if(F=="currency-3")return FLinearColor(0.957,0.706,0.812);
  if(F=="currency-special")return FLinearColor(0.949,0.757,0.471);
  if(F=="omen")return FLinearColor(0.773,0.718,0.957);
  if(F=="magic")return FLinearColor(0.49,0.682,1);
  if(F=="rare")return FLinearColor(1,0.824,0.388);
  if(F=="legendary")return FLinearColor(1,0.608,0.298);
  if(F=="unique")return FLinearColor(0.784,0.565,1);
  return FLinearColor(0.71,0.749,0.812);
 }
 FVector Position(FOpusEntry E,double Age,double Seed)
 {
  double Flight=0.68+Seed*0.18,U=Math::Clamp(Age/Flight,0.0,1.0);
  FVector P=E.SourcePosition+(E.Position-E.SourcePosition)*U;
  P.Z=48*(1-U)+13*U+4*(165+Seed*80)*U*(1-U);
  if(Age>Flight&&Age<Flight+0.2)P.Z+=Math::Sin((Age-Flight)/0.2*PI)*13;
  return P;
 }
 bool Landed(double Age,double Seed){return Age>=0.88+Seed*0.18;}
}
class ADWOFloorLootMarker : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Item;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Beam;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Shadow;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Ring;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Trail0;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Trail1;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Trail2;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Trail3;
 UPROPERTY() FOpusEntry Entry;
 UPROPERTY() TArray<UStaticMeshComponent> Trails;
 double Born=0,Seed=0;
 void Initialize(FOpusEntry E,double Time)
 {
  Entry=E;Born=Time;Seed=UOpusWorldUI::PresentationSeed(E.Key);
  auto Cube=Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube"));
  auto Cylinder=Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cylinder.Cylinder"));
  auto Sphere=Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Sphere.Sphere"));
  Trails.Add(Trail0);Trails.Add(Trail1);Trails.Add(Trail2);Trails.Add(Trail3);
  TArray<UStaticMeshComponent> Meshes=Trails;Meshes.Add(Item);Meshes.Add(Beam);Meshes.Add(Shadow);Meshes.Add(Ring);
  for(auto M:Meshes){M.SetCollisionProfileName(n"NoCollision");M.SetCastShadow(false);UOpusNative::Tint(M,DWOLoot::Color(E));}
  Item.SetStaticMesh(Cube);Item.SetRelativeScale3D(E.Kind=="equipment"?FVector(0.28,0.2,0.13):FVector(0.14,0.14,0.22));
  if(E.Kind!="equipment")Item.SetRelativeRotation(FRotator(0,45,45));
  Beam.SetStaticMesh(Cylinder);Beam.SetRelativeScale3D(FVector(0.025,0.025,0.65));
  Shadow.SetStaticMesh(Cylinder);Shadow.SetRelativeScale3D(FVector(0.28,0.21,0.005));UOpusNative::Tint(Shadow,FLinearColor(0.025,0.03,0.04));
  Ring.SetStaticMesh(Cylinder);Ring.SetRelativeScale3D(FVector(0.32,0.32,0.004));
  for(int I=0;I<Trails.Num();I++){Trails[I].SetStaticMesh(Sphere);Trails[I].SetRelativeScale3D(FVector(0.045*(1.0-I*0.15)));}
 }
 void Update(double Time,bool Shown)
 {
  SetActorHiddenInGame(!Shown);if(!Shown)return;double Age=Math::Max(0.0,Time-Born),Flight=0.68+Seed*0.18;
  FVector At=DWOLoot::Position(Entry,Age,Seed);Item.SetWorldLocation(At);Item.SetWorldRotation(FRotator(Age<Flight?Age*270:0,Seed*360+Age*40,Entry.Kind=="equipment"?0:45));
  Shadow.SetWorldLocation(Entry.Position+FVector(0,0,1));
  bool Landed=DWOLoot::Landed(Age,Seed);Beam.SetVisibility(Landed);Ring.SetVisibility(Age>=Flight&&Age<Flight+0.32);
  Beam.SetWorldLocation(Entry.Position+FVector(0,0,45));Ring.SetWorldLocation(Entry.Position+FVector(0,0,2));
  for(int I=0;I<Trails.Num();I++){Trails[I].SetVisibility(Age<Flight);Trails[I].SetWorldLocation(DWOLoot::Position(Entry,Math::Max(0.0,Age-(I+1)*0.035),Seed));}
 }
}

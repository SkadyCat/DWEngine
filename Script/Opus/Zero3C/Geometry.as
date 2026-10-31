namespace DWO3CGeometry
{
 // Physical Pawn capsules block the WorldStatic trace channel. Geometry queries
 // must instead select actual static objects; DWRules owns combat hit spheres.
 double Sweep(FVector Start,FVector End,double Radius)
 {
  TArray<EObjectTypeQuery> Types;Types.Add(EObjectTypeQuery::WorldStatic);
  TArray<AActor> Ignore;FHitResult Hit;
  if(System::SphereTraceSingleForObjects(Start,End,float32(Math::Max(1.0,Radius)),Types,false,Ignore,EDrawDebugTrace::None,Hit,true))return Hit.Time;
  return -1;
 }
}

// RuleStudio atlas-state port: navigation/progress only. Tower combat is unchanged.
struct FDWOAtlasNode
{
 FString Id,Name,Biome,Status;
 int Q=0,R=0,Depth=0,Floor=1;
 FVector2D Position;
 TArray<FString> Neighbors;
}
class UDWOAtlasState : UObject
{
 uint Seed=1;
 FString Selected="0:0",Active,Current="0:0";
 TArray<FString> Completed;
 TMap<FString,int> Attempts;
 TMap<FString,int> MapTiers;
 TArray<FDWOAtlasNode> Nodes;
 TMap<FString,int> Indexes;
 uint GeometrySeed=0;
 bool GeometryReady=false;
 uint Hash(FString Text) const {uint V=2166136261;for(int I=0;I<Text.Len();I++){V^=uint(Text[I]);V*=16777619;}V^=V>>16;V*=0x7feb352d;V^=V>>15;return V;}
 int Ring(int Q,int R) const{return Math::Max(Math::Abs(Q),Math::Max(Math::Abs(R),Math::Abs(Q+R)));}
 FString Id(int Q,int R) const{return ""+Q+":"+R;}
 int DQ(int I) const{return I==0||I==5?1:I==2||I==3?-1:0;}
 int DR(int I) const{return I==1||I==2?1:I==4||I==5?-1:0;}
 bool Coordinates(FString Value,int& Q,int& R) const
 {TArray<FString> P;Value.ParseIntoArray(P,":",false);if(P.Num()!=2||!P[0].IsNumeric()||!P[1].IsNumeric())return false;Q=String::Conv_StringToInt(P[0]);R=String::Conv_StringToInt(P[1]);return Math::Abs(Q)<=100000&&Math::Abs(R)<=100000&&Id(Q,R)==Value;}
 FString Parent(int Q,int R) const
 {
  int Depth=Ring(Q,R);if(Depth==0)return "";TArray<FString> Parents;
  for(int D=0;D<6;D++)if(Ring(Q+DQ(D),R+DR(D))==Depth-1)Parents.Add(Id(Q+DQ(D),R+DR(D)));
  return Parents[int(Hash(""+Seed+"|"+Q+"|"+R+"|parent")%uint(Parents.Num()))];
 }
 TArray<FString> Neighbors(int Q,int R) const
 {
  TArray<FString> Out;auto A=Id(Q,R);
  for(int D=0;D<6;D++){int X=Q+DQ(D),Y=R+DR(D);auto B=Id(X,Y);FString Left=A<B?A:B,Right=A<B?B:A;
   if(Parent(Q,R)==B||Parent(X,Y)==A||Hash(""+Seed+"|"+Left+"|"+Right+"|link")%100<32)Out.Add(B);
  }return Out;
 }
 int Index(FString Value) const {return Indexes.Contains(Value)?Indexes[Value]:-1;}
 FDWOAtlasNode Describe(int Q,int R) const
 {
  FDWOAtlasNode N;N.Q=Q;N.R=R;N.Id=Id(Q,R);N.Depth=Ring(Q,R);N.Floor=N.Id=="0:0"?1:0;
  FString Region=""+int(Math::FloorToInt(double(Q)/4))+":"+int(Math::FloorToInt(double(R)/4));
  int Biome=int(Hash(""+Seed+"|"+Region+"|biome")%6),Name=int(Hash(""+Seed+"|"+N.Id+"|name")%4);
  TArray<FString> Names;
  if(Biome==0){N.Biome="密林";Names.Add("苍翠林地");Names.Add("暮色深林");Names.Add("古木之境");Names.Add("幽影密林");}
  else if(Biome==1){N.Biome="沼泽";Names.Add("雾隐泽地");Names.Add("沉寂泥沼");Names.Add("苔痕湿地");Names.Add("灰雾浅滩");}
  else if(Biome==2){N.Biome="遗迹";Names.Add("失落遗迹");Names.Add("断壁回廊");Names.Add("古城残垣");Names.Add("寂静庭院");}
  else if(Biome==3){N.Biome="荒漠";Names.Add("金砂荒原");Names.Add("风蚀丘陵");Names.Add("落日沙海");Names.Add("赤岩旷野");}
  else if(Biome==4){N.Biome="雪域";Names.Add("寒霜山麓");Names.Add("银白冻原");Names.Add("雪落高地");Names.Add("冰封峡谷");}
  else {N.Biome="海岸";Names.Add("潮汐海岸");Names.Add("碎礁湾");Names.Add("静水长滩");Names.Add("苍蓝群岛");}
  N.Name=N.Depth==0?"初始界域":Names[Name];N.Position=FVector2D(Q*142+R*71+(double(Hash(""+Seed+"|"+N.Id+"|x"))/4294967296.0-.5)*68,R*123+(double(Hash(""+Seed+"|"+N.Id+"|y"))/4294967296.0-.5)*68);N.Neighbors=Neighbors(Q,R);return N;
 }
 void AddRegion(int CQ,int CR,int Radius,TMap<FString,bool>& Locations)
 {for(int Q=-Radius;Q<=Radius;Q++)for(int R=Math::Max(-Radius,-Q-Radius);R<=Math::Min(Radius,-Q+Radius);R++)Locations.Add(Id(CQ+Q,CR+R),true);}
 void Build()
 {
  TMap<FString,bool> Locations;AddRegion(0,0,10,Locations);for(auto C:Completed){int Q=0,R=0;if(Coordinates(C,Q,R))AddRegion(Q,R,4,Locations);}
  // Coordinates/links are immutable for a seed. A status change must not hash
  // the entire world again on the same frame as a UI transaction.
  TArray<FDWOAtlasNode> Next;
  for(auto Pair:Locations){if(GeometryReady&&GeometrySeed==Seed&&Indexes.Contains(Pair.Key))Next.Add(Nodes[Indexes[Pair.Key]]);else {int Q=0,R=0;Coordinates(Pair.Key,Q,R);Next.Add(Describe(Q,R));}}
  Nodes=Next;Indexes.Empty();for(int I=0;I<Nodes.Num();I++)Indexes.Add(Nodes[I].Id,I);GeometrySeed=Seed;GeometryReady=true;
  TMap<FString,int> Seen;TArray<FString> Queue;TArray<int> Remaining;Queue.Add("0:0");Remaining.Add(3);for(auto C:Completed){Queue.Add(C);Remaining.Add(2);}
  for(int At=0;At<Queue.Num();At++){auto Key=Queue[At];int Left=Remaining[At];if(Seen.Contains(Key)&&Seen[Key]>=Left)continue;Seen.Add(Key,Left);int I=Index(Key);if(I<0||Left<=0)continue;for(auto B:Nodes[I].Neighbors){Queue.Add(B);Remaining.Add(Left-1);}}
  for(int I=0;I<Nodes.Num();I++){
   auto N=Nodes[I];N.Floor=MapTiers.Contains(N.Id)?MapTiers[N.Id]:N.Id=="0:0"?1:0;bool Available=Completed.Num()==0?N.Id=="0:0":false;for(auto B:N.Neighbors)if(Completed.Contains(B))Available=true;
   N.Status=N.Id==Active?"active":Completed.Contains(N.Id)?"completed":Available?Attempts.Contains(N.Id)?"failed":"available":Seen.Contains(N.Id)?"locked":"hidden";Nodes[I]=N;
  }
  int SelectedIndex=Index(Selected);if(SelectedIndex<0||Nodes[SelectedIndex].Status=="hidden")Selected=Active.IsEmpty()?"0:0":Active;
 }
 FString Serialize() const
 {
  FString S="opus_atlas|1|"+Seed+"\nselected|"+Selected+"\nactive|"+Active+"\ncurrent|"+Current+"\n";
  for(auto C:Completed)S+="cleared|"+C+"\n";for(auto A:Attempts)S+="attempt|"+A.Key+"|"+A.Value+"\n";for(auto T:MapTiers)S+="map-tier|"+T.Key+"|"+T.Value+"\n";return S;
 }
 bool Restore(FString S)
 {
  Completed.Empty();Attempts.Empty();MapTiers.Empty();Active="";Current="";Selected="0:0";Seed=1;if(S.IsEmpty()){Current="0:0";Build();return true;}
  TArray<FString> Lines;S.ParseIntoArray(Lines,"\n",true);if(Lines.Num()<1||Lines.Num()>300005)return false;
  TArray<FString> Header;Lines[0].ParseIntoArray(Header,"|",false);if(Header.Num()!=3||Header[0]!="opus_atlas"||Header[1]!="1"||!Header[2].IsNumeric())return false;Seed=uint(String::Conv_StringToInt64(Header[2]));
  for(int I=1;I<Lines.Num();I++){
   TArray<FString> P;Lines[I].ParseIntoArray(P,"|",false);if(P.Num()<2)return false;int Q=0,R=0;if(!P[1].IsEmpty()&&!Coordinates(P[1],Q,R))return false;
   if(P[0]=="selected")Selected=P[1];else if(P[0]=="active")Active=P[1];else if(P[0]=="current")Current=P[1];else if(P[0]=="cleared"){if(P[1].IsEmpty()||Completed.Contains(P[1]))return false;Completed.Add(P[1]);}
   else if(P[0]=="attempt"){if(P.Num()!=3||!P[2].IsNumeric()||String::Conv_StringToInt(P[2])<1)return false;Attempts.Add(P[1],String::Conv_StringToInt(P[2]));}
   else if(P[0]=="map-tier"){if(P.Num()!=3||!P[2].IsNumeric()||MapTiers.Contains(P[1]))return false;int Tier=String::Conv_StringToInt(P[2]);if(Tier<1||Tier>16)return false;MapTiers.Add(P[1],Tier);}else return false;
  }
  if(Completed.Num()>0){if(!Completed.Contains("0:0"))return false;TArray<FString> Seen;Seen.Add("0:0");for(int At=0;At<Seen.Num();At++){int Q=0,R=0;Coordinates(Seen[At],Q,R);for(auto B:Neighbors(Q,R))if(Completed.Contains(B)&&!Seen.Contains(B))Seen.Add(B);}if(Seen.Num()!=Completed.Num())return false;}
  for(auto T:MapTiers)if(!Attempts.Contains(T.Key))return false;
  if(Current.IsEmpty())Current=!Active.IsEmpty()?Active:Completed.Num()>0?Completed.Last():"0:0";Build();return Active.IsEmpty()||(Index(Active)>=0&&(!Completed.Contains(Active)||Active=="0:0")&&Attempts.Contains(Active));
 }
 bool Select(FString Value){int I=Index(Value);if(I<0||Nodes[I].Status=="hidden")return false;Selected=Value;return true;}
 bool Enter(int MapTier=1){int I=Index(Selected);if(MapTier<1||MapTier>16||!Active.IsEmpty()||I<0||(Nodes[I].Status!="available"&&Nodes[I].Status!="failed"&&!(Selected=="0:0"&&Nodes[I].Status=="completed")))return false;Active=Selected;Current=Active;Attempts.Add(Active,Attempts.Contains(Active)?Attempts[Active]+1:1);MapTiers.Add(Active,MapTier);Build();return true;}
 void Complete(){if(Active.IsEmpty())return;Completed.AddUnique(Active);Selected=Active;Active="";Build();}
 void Fail(){if(Active.IsEmpty())return;Selected=Active;Active="";Build();}
}

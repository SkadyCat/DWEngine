#pragma once
#include "OpusUI.h"
class UEditableTextBox;
// Runtime values use the saved SVG nodes and their authored row prefabs.
namespace OpusSvg
{
void Text(USvgCanvasNode* Node,const FString& Value);
void Image(USvgCanvasNode* Node,const FString& Path);
void Progress(USvgCanvasNode* Node,float Value);
void Rows(USvgCanvasScreen* Screen,USvgCanvasNode* Node,const FString& Binding,const TArray<FOpusUIRow>& Values);
void Action(USvgCanvasNode* Node,const FString& Op,const FString& Value);
void Input(USvgCanvasNode* Node,UEditableTextBox* Edit);
USvgCanvasNode* DuplicateTree(USvgCanvasScreen* Screen,USvgCanvasNode* Source,UCanvasPanel* Parent);
float TextHeight(USvgCanvasNode* Source,const FString& Value,float Width,float Extra=8);
void PlaceNode(USvgCanvasNode* Node,float X,float Y,float Width,float Height);
void ArrangeRows(USvgCanvasScreen* Screen,USvgCanvasNode* Node,const FString& Binding,const TArray<FVector4>& Rects,const TArray<float>& Angles);
}

// C++ native tools - Behavior Tree, Animation, UMG, Input, Physics
#pragma once
#include "CoreMinimal.h"

class UE5AIAUTO_API FUE5AIAUTOCppTools
{
public:
	// === Behavior Tree ===
	static TSharedPtr<FJsonObject> CreateBehaviorTree(const FString& Path, const FString& Name);
	static TSharedPtr<FJsonObject> AddBTComposite(const FString& Path, const FString& ParentId, const FString& CompositeType);
	static TSharedPtr<FJsonObject> AddBTTask(const FString& Path, const FString& ParentId, const FString& TaskClass);
	static TSharedPtr<FJsonObject> AddBTDecorator(const FString& Path, const FString& NodeId, const FString& DecoratorClass);
	static TSharedPtr<FJsonObject> AddBTService(const FString& Path, const FString& CompositeId, const FString& ServiceClass);
	static TSharedPtr<FJsonObject> CreateBlackboard(const FString& Path, const FString& Name);
	static TSharedPtr<FJsonObject> AddBlackboardKey(const FString& Path, const FString& KeyName, const FString& KeyType);

	// === Animation Blueprint ===
	static TSharedPtr<FJsonObject> AddAnimState(const FString& AnimBP, const FString& StateName, double X, double Y);
	static TSharedPtr<FJsonObject> AddAnimTransition(const FString& AnimBP, const FString& FromState, const FString& ToState, const FString& Condition);
	static TSharedPtr<FJsonObject> SetAnimGraphNode(const FString& AnimBP, const FString& NodeClass, double X, double Y);

	// === UMG Widget ===
	static TSharedPtr<FJsonObject> CreateWidgetBlueprint(const FString& Path, const FString& Name);
	static TSharedPtr<FJsonObject> AddWidgetToCanvas(const FString& Path, const FString& WidgetClass, const FString& Name);
	static TSharedPtr<FJsonObject> WidgetSetText(const FString& Path, const FString& WidgetName, const FString& Text);
	static TSharedPtr<FJsonObject> WidgetSetFont(const FString& Path, const FString& WidgetName, int32 FontSize, const FString& ColorHex);
	static TSharedPtr<FJsonObject> WidgetListTree(const FString& Path);
	static TSharedPtr<FJsonObject> WidgetSetPosition(const FString& Path, const FString& WidgetName, double X, double Y, double W, double H);
	static TSharedPtr<FJsonObject> WidgetAddToViewport(const FString& Path);
	static TSharedPtr<FJsonObject> WidgetSetVisibility(const FString& Path, const FString& WidgetName, const FString& Visibility);

	// === Input Mappings ===
	static TSharedPtr<FJsonObject> AddActionMapping(const FString& ActionName, const FString& Key);
	static TSharedPtr<FJsonObject> AddAxisMapping(const FString& AxisName, const FString& Key, float Scale);

	// === Physics ===
	static TSharedPtr<FJsonObject> EnablePhysics(const FString& ActorName, bool bEnable);
	static TSharedPtr<FJsonObject> SetMass(const FString& ActorName, float Mass);
	static TSharedPtr<FJsonObject> ApplyForce(const FString& ActorName, double X, double Y, double Z);

	// === Audio ===
	static TSharedPtr<FJsonObject> PlaySound(const FString& ActorName, const FString& SoundPath);
	static TSharedPtr<FJsonObject> StopSound(const FString& ActorName);

	// === Navigation ===
	static TSharedPtr<FJsonObject> FindPathTo(const FString& ActorName, double X, double Y, double Z);

	// === DataTable ===
	static TSharedPtr<FJsonObject> CreateDataTable(const FString& Path, const FString& Name, const FString& RowStructPath);
	static TSharedPtr<FJsonObject> AddDataTableRow(const FString& Path, const FString& RowName, const TSharedPtr<FJsonObject>& RowData);
	static TSharedPtr<FJsonObject> GetDataTableRow(const FString& Path, const FString& RowName);
};
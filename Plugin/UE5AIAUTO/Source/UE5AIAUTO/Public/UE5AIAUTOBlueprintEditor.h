// Copyright GU5. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

class UE5AIAUTO_API FUE5AIAUTOBlueprintEditor
{
public:
	// === Blueprint CRUD ===
	static TSharedPtr<FJsonObject> ListBlueprints(const FString& BasePath);
	static TSharedPtr<FJsonObject> OpenBlueprint(const FString& Path);
	static TSharedPtr<FJsonObject> CreateBlueprint(const FString& Path, const FString& ParentClass);
	static TSharedPtr<FJsonObject> CompileBlueprint(const FString& Path);

	// === Node operations ===
	static TSharedPtr<FJsonObject> ListNodes(const FString& BP, const FString& Graph);
	static TSharedPtr<FJsonObject> CreateNode(const FString& BP, const FString& Graph, const FString& NodeClass, double X, double Y, TSharedPtr<FJsonObject> Defaults);
	static TSharedPtr<FJsonObject> RemoveNode(const FString& BP, const FString& Graph, const FString& NodeId);
	static TSharedPtr<FJsonObject> SetPinDefault(const FString& BP, const FString& Graph, const FString& NodeId, const FString& PinName, const FString& Value);
	static TSharedPtr<FJsonObject> ConnectPins(const FString& BP, const FString& Graph, const FString& SrcNode, const FString& SrcPin, const FString& DstNode, const FString& DstPin);
	static TSharedPtr<FJsonObject> DisconnectPin(const FString& BP, const FString& Graph, const FString& NodeId, const FString& PinName);

	// === Variable operations ===
	static TSharedPtr<FJsonObject> ListVariables(const FString& BP);
	static TSharedPtr<FJsonObject> CreateVariable(const FString& BP, const FString& Name, const FString& Type, bool bIsArray);
	static TSharedPtr<FJsonObject> RemoveVariable(const FString& BP, const FString& Name);
	static TSharedPtr<FJsonObject> SetVariableDefault(const FString& BP, const FString& Name, const FString& Value);

	// === Function/Graph operations ===
	static TSharedPtr<FJsonObject> ListFunctions(const FString& BP);
	static TSharedPtr<FJsonObject> CreateFunction(const FString& BP, const FString& Name, TArray<TSharedPtr<FJsonValue>> Inputs, TArray<TSharedPtr<FJsonValue>> Outputs);
	static TSharedPtr<FJsonObject> RemoveFunction(const FString& BP, const FString& Name);
	static TSharedPtr<FJsonObject> ListGraphs(const FString& BP);

	// === Component operations ===
	static TSharedPtr<FJsonObject> ListComponents(const FString& BP);
	static TSharedPtr<FJsonObject> AddComponent(const FString& BP, const FString& ComponentClass, const FString& VariableName);
	static TSharedPtr<FJsonObject> RemoveComponent(const FString& BP, const FString& Name);

	// === Interface operations ===
	static TSharedPtr<FJsonObject> ListInterfaces(const FString& BP);
	static TSharedPtr<FJsonObject> AddInterface(const FString& BP, const FString& InterfaceClass);
	static TSharedPtr<FJsonObject> RemoveInterface(const FString& BP, const FString& InterfaceClass);

	// === Macro operations ===
	static TSharedPtr<FJsonObject> ListMacros(const FString& BP);
	static TSharedPtr<FJsonObject> CreateMacro(const FString& BP, const FString& Name);
	static TSharedPtr<FJsonObject> RemoveMacro(const FString& BP, const FString& Name);

	// === Delegate/Event Dispatcher ===
	static TSharedPtr<FJsonObject> AddEventDispatcher(const FString& BP, const FString& Name);
	static TSharedPtr<FJsonObject> RemoveEventDispatcher(const FString& BP, const FString& Name);

	// === Tools ===
	static TSharedPtr<FJsonObject> ListNodeClasses();
	static TSharedPtr<FJsonObject> SaveBlueprint(const FString& Path);

private:
	static TMap<FString, UBlueprint*> LoadedBPs;
	static UBlueprint* GetCachedBP(const FString& Path);
};

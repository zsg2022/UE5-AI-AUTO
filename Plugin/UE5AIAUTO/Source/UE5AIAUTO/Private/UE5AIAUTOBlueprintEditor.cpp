// Copyright GU5. All Rights Reserved.
// UE5 AI Plugin - Blueprint Editor Engine
#include "UE5AIAUTOBlueprintEditor.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/UObjectIterator.h"

TMap<FString, UBlueprint*> FUE5AIAUTOBlueprintEditor::LoadedBPs;
static TSharedPtr<FJsonObject> JO(){return MakeShareable(new FJsonObject());}
static TSharedPtr<FJsonValue> JOV(TSharedPtr<FJsonObject> J){return MakeShareable(new FJsonValueObject(J));}
#define ERR(M) do{auto E=JO();E->SetStringField(TEXT("error"),M);return E;}while(0)

UBlueprint* FUE5AIAUTOBlueprintEditor::GetCachedBP(const FString& Path){
	UBlueprint** Found=LoadedBPs.Find(Path);if(Found)return *Found;
	UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Path);if(!BP){FString FullPath=FPaths::GetBaseFilename(Path);BP=LoadObject<UBlueprint>(nullptr,*(Path+TEXT(".")+FullPath));}
	if(BP)LoadedBPs.Add(Path,BP);
	return BP;
}
static UEdGraph* _Graph(UBlueprint* B,const FString& N){
	for(UEdGraph* G:B->UbergraphPages)if(G->GetName()==N)return G;
	for(UEdGraph* G:B->FunctionGraphs)if(G->GetName()==N)return G;
	for(UEdGraph* G:B->MacroGraphs)if(G->GetName()==N)return G;
	return nullptr;
}
static UK2Node* _Node(UEdGraph* G,const FString& Id){
	for(UEdGraphNode* N:G->Nodes)if(N->NodeGuid.ToString()==Id)return Cast<UK2Node>(N);
	return nullptr;
}
static UEdGraphPin* _Pin(UK2Node* N,const FString& Name){
	for(UEdGraphPin* P:N->Pins)if(P->PinName.ToString()==Name)return P;
	return nullptr;
}
static void EnsureClass(UBlueprint* B){if(!B->GeneratedClass)FKismetEditorUtilities::CompileBlueprint(B);}
static UFunction* _Function(const FString& Name){
	FString OwnerName, FunctionName = Name;
	if(Name.Split(TEXT("."), &OwnerName, &FunctionName) || Name.Split(TEXT(":"), &OwnerName, &FunctionName)){
		for(TObjectIterator<UClass> It;It;++It)if(It->GetName()==OwnerName||It->GetName()==OwnerName.RightChop(1)){
			if(UFunction* F=It->FindFunctionByName(FName(*FunctionName)))return F;
		}
	}
	if(UFunction* F=FindObject<UFunction>(nullptr,*Name,true))return F;
	for(TObjectIterator<UClass> It;It;++It)if(UFunction* F=It->FindFunctionByName(FName(*FunctionName)))return F;
	for(TObjectIterator<UFunction> It;It;++It)if(It->GetName()==FunctionName)return *It;
	return nullptr;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListNodeClasses(){
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(TObjectIterator<UClass> It;It;++It)if(It->IsChildOf(UK2Node::StaticClass())&&!(It->ClassFlags&CLASS_Abstract))A.Add(MakeShareable(new FJsonValueString(It->GetName())));
	R->SetArrayField(TEXT("node_classes"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListBlueprints(const FString& Path){
	auto& ARM=FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter F;F.PackagePaths.Add(FName(*Path));F.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());F.bRecursivePaths=true;
	TArray<FAssetData> A;ARM.Get().GetAssets(F,A);
	auto R=JO();TArray<TSharedPtr<FJsonValue>> Arr;
	for(const auto& D:A){auto J=JO();J->SetStringField(TEXT("name"),D.AssetName.ToString());J->SetStringField(TEXT("path"),D.GetObjectPathString());Arr.Add(JOV(J));}
	R->SetArrayField(TEXT("blueprints"),Arr);R->SetNumberField(TEXT("count"),Arr.Num());return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::OpenBlueprint(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();R->SetStringField(TEXT("name"),B->GetName());
	R->SetStringField(TEXT("parent"),B->ParentClass?B->ParentClass->GetName():TEXT("None"));
	TArray<TSharedPtr<FJsonValue>> GA;
	for(UEdGraph* G:B->UbergraphPages){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("type"),TEXT("EventGraph"));GA.Add(JOV(J));}
	for(UEdGraph* G:B->FunctionGraphs){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("type"),TEXT("Function"));GA.Add(JOV(J));}
	for(UEdGraph* G:B->MacroGraphs){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("type"),TEXT("Macro"));GA.Add(JOV(J));}
	R->SetArrayField(TEXT("graphs"),GA);
	auto V=ListVariables(P);if(V)R->SetArrayField(TEXT("variables"),V->GetArrayField(TEXT("variables")));
	auto C=ListComponents(P);if(C)R->SetArrayField(TEXT("components"),C->GetArrayField(TEXT("components")));
	return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::CreateBlueprint(const FString& Path,const FString& PC){
	UClass* P=FindObject<UClass>(nullptr,*PC);if(!P)P=AActor::StaticClass();
	FString N=FPaths::GetBaseFilename(Path),F=FPaths::GetPath(Path);
	UPackage* Pkg=CreatePackage(*(F/N));
	UBlueprintFactory* Fac=NewObject<UBlueprintFactory>();Fac->ParentClass=P;
	UBlueprint* BP=Cast<UBlueprint>(Fac->FactoryCreateNew(UBlueprint::StaticClass(),Pkg,*N,RF_Standalone|RF_Public,nullptr,GWarn));
	if(!BP)ERR("Create failed");
	LoadedBPs.Add(F/N,BP);Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(BP);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("created"));R->SetStringField(TEXT("path"),F/N);return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::CompileBlueprint(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	FKismetEditorUtilities::CompileBlueprint(B);
	auto R=JO();R->SetStringField(TEXT("status"),B->Status==BS_UpToDate?TEXT("ok"):TEXT("has_errors"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::SaveBlueprint(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	B->GetOutermost()->SetDirtyFlag(true);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("dirty"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListNodes(const FString& P,const FString& GName){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("BP not found");
	UEdGraph* G=_Graph(B,GName);if(!G)ERR("Graph not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(UEdGraphNode* N:G->Nodes){
		auto J=JO();J->SetStringField(TEXT("id"),N->NodeGuid.ToString());J->SetStringField(TEXT("class"),N->GetClass()->GetName());
		J->SetStringField(TEXT("title"),N->GetNodeTitle(ENodeTitleType::ListView).ToString());
		J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);
		TArray<TSharedPtr<FJsonValue>> PA;
		for(UEdGraphPin* Pin:N->Pins){auto PJ=JO();PJ->SetStringField(TEXT("name"),Pin->PinName.ToString());PJ->SetStringField(TEXT("type"),Pin->PinType.PinCategory.ToString());PJ->SetStringField(TEXT("dir"),Pin->Direction==EGPD_Input?TEXT("In"):TEXT("Out"));if(Pin->DefaultValue.Len())PJ->SetStringField(TEXT("default"),Pin->DefaultValue);PJ->SetNumberField(TEXT("connected"),Pin->LinkedTo.Num());PA.Add(JOV(PJ));}
		J->SetArrayField(TEXT("pins"),PA);A.Add(JOV(J));}
	R->SetArrayField(TEXT("nodes"),A);R->SetNumberField(TEXT("count"),A.Num());
	return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::CreateNode(const FString& P,const FString& GName,const FString& NC,double X,double Y,TSharedPtr<FJsonObject> Defs){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("BP not found");
	UEdGraph* G=_Graph(B,GName);
	if(!G){G=FBlueprintEditorUtils::CreateNewGraph(B,*GName,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());if(GName!=TEXT("EventGraph"))FBlueprintEditorUtils::AddFunctionGraph<UClass>(B,G,false,nullptr);else{B->UbergraphPages.Add(G);G->bAllowDeletion=false;}}
	UClass* NClass=FindObject<UClass>(nullptr,*(TEXT("/Script/BlueprintGraph.")+NC));
	if(!NClass)for(TObjectIterator<UClass> It;It;++It)if(It->IsChildOf(UK2Node::StaticClass())&&It->GetName()==NC){NClass=*It;break;}
	if(!NClass||!NClass->IsChildOf(UK2Node::StaticClass()))ERR(FString::Printf(TEXT("Node class not found: %s"),*NC));
	UK2Node* Node=NewObject<UK2Node>(G,NClass);Node->NodePosX=X;Node->NodePosY=Y;G->AddNode(Node);if(!Node->NodeGuid.IsValid())Node->CreateNewGuid();
	if(Defs.IsValid()){
		if(Defs->HasField(TEXT("function"))){if(auto* CF=Cast<UK2Node_CallFunction>(Node)){if(auto* UF=_Function(Defs->GetStringField(TEXT("function"))))CF->SetFromFunction(UF);else{G->RemoveNode(Node);ERR(FString::Printf(TEXT("Function not found: %s"),*Defs->GetStringField(TEXT("function"))));}}}
		if(Defs->HasField(TEXT("event"))){FString Ev=Defs->GetStringField(TEXT("event"));if(auto* EN=Cast<UK2Node_Event>(Node)){if(Ev==TEXT("BeginPlay"))EN->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"),AActor::StaticClass());else if(Ev==TEXT("Tick"))EN->EventReference.SetExternalMember(TEXT("ReceiveTick"),AActor::StaticClass());}}
		if(Defs->HasField(TEXT("variable"))){FString Vn=Defs->GetStringField(TEXT("variable"));if(auto* VG=Cast<UK2Node_VariableGet>(Node))VG->VariableReference.SetSelfMember(FName(*Vn));if(auto* VS=Cast<UK2Node_VariableSet>(Node))VS->VariableReference.SetSelfMember(FName(*Vn));}
	}
	Node->AllocateDefaultPins();Node->ReconstructNode();FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("id"),Node->NodeGuid.ToString());R->SetStringField(TEXT("class"),Node->GetClass()->GetName());return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveNode(const FString& P,const FString& GName,const FString& Id){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("BP not found");UEdGraph* G=_Graph(B,GName);if(!G)ERR("Graph not found");
	UK2Node* N=_Node(G,Id);if(!N)ERR("Node not found");G->RemoveNode(N);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::SetPinDefault(const FString& P,const FString& GName,const FString& NId,const FString& PinName,const FString& Val){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("BP not found");UEdGraph* G=_Graph(B,GName);if(!G)ERR("Graph not found");
	UK2Node* N=_Node(G,NId);if(!N)ERR("Node not found");UEdGraphPin* Pin=_Pin(N,PinName);if(!Pin)ERR("Pin not found");
	Pin->DefaultValue=Val;FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("set"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ConnectPins(const FString& P,const FString& GName,const FString& SN,const FString& SP,const FString& DN,const FString& DP){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("BP not found");UEdGraph* G=_Graph(B,GName);if(!G)ERR("Graph not found");
	UK2Node* S=_Node(G,SN),*D=_Node(G,DN);if(!S||!D)ERR("Node not found");
	UEdGraphPin* Ps=_Pin(S,SP),*Pd=_Pin(D,DP);if(!Ps||!Pd)ERR("Pin not found");
	if(Ps->Direction==Pd->Direction)ERR("Same direction");
	G->GetSchema()->TryCreateConnection(Ps,Pd);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("connected"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::DisconnectPin(const FString& P,const FString& GName,const FString& NId,const FString& PinName){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("BP not found");UEdGraph* G=_Graph(B,GName);if(!G)ERR("Graph not found");
	UK2Node* N=_Node(G,NId);if(!N)ERR("Node not found");UEdGraphPin* Pin=_Pin(N,PinName);if(!Pin)ERR("Pin not found");
	Pin->BreakAllPinLinks();FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("disconnected"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListVariables(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(const auto& V:B->NewVariables){auto J=JO();J->SetStringField(TEXT("name"),V.VarName.ToString());J->SetStringField(TEXT("type"),V.VarType.PinCategory.ToString());J->SetStringField(TEXT("default"),V.DefaultValue);A.Add(JOV(J));}
	R->SetArrayField(TEXT("variables"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::CreateVariable(const FString& P,const FString& N,const FString& T,bool bA){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	FEdGraphPinType PT;bool bValid=true;
	if(T==TEXT("int")||T==TEXT("int32"))PT.PinCategory=UEdGraphSchema_K2::PC_Int;
	else if(T==TEXT("float")){PT.PinCategory=UEdGraphSchema_K2::PC_Real;PT.PinSubCategory=UEdGraphSchema_K2::PC_Float;}
	else if(T==TEXT("double")){PT.PinCategory=UEdGraphSchema_K2::PC_Real;PT.PinSubCategory=UEdGraphSchema_K2::PC_Double;}
	else if(T==TEXT("bool"))PT.PinCategory=UEdGraphSchema_K2::PC_Boolean;
	else if(T==TEXT("string"))PT.PinCategory=UEdGraphSchema_K2::PC_String;
	else if(T==TEXT("text"))PT.PinCategory=UEdGraphSchema_K2::PC_Text;
	else if(T==TEXT("name"))PT.PinCategory=UEdGraphSchema_K2::PC_Name;
	else if(T==TEXT("vector"))PT.PinCategory=UEdGraphSchema_K2::PC_Struct;
	else{UClass* C=FindObject<UClass>(nullptr,*T);if(C){PT.PinCategory=UEdGraphSchema_K2::PC_Object;PT.PinSubCategoryObject=C;}else bValid=false;}
	if(!bValid)ERR(FString::Printf(TEXT("Unknown type: %s"),*T));
	PT.ContainerType=bA?EPinContainerType::Array:EPinContainerType::None;
	// Directly add to NewVariables - bypass crashy AddMemberVariable
	FBPVariableDescription VD;VD.VarName=FName(*N);VD.VarType=PT;
	VD.Category=FText();VD.PropertyFlags|=CPF_Edit|CPF_BlueprintVisible|CPF_DisableEditOnInstance;
	B->NewVariables.Add(VD);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("created"));R->SetStringField(TEXT("name"),N);return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveVariable(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	for(int32 i=0;i<B->NewVariables.Num();++i)if(B->NewVariables[i].VarName==*N){B->NewVariables.RemoveAt(i);FBlueprintEditorUtils::MarkBlueprintAsModified(B);auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;}
	ERR("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::SetVariableDefault(const FString& P,const FString& N,const FString& V){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	for(FBPVariableDescription& VD:B->NewVariables)if(VD.VarName==*N){VD.DefaultValue=V;FBlueprintEditorUtils::MarkBlueprintAsModified(B);auto R=JO();R->SetStringField(TEXT("status"),TEXT("set"));return R;}
	ERR("Not found");
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListFunctions(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(UEdGraph* G:B->FunctionGraphs){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetNumberField(TEXT("nodes"),G->Nodes.Num());A.Add(JOV(J));}
	R->SetArrayField(TEXT("functions"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::CreateFunction(const FString& P,const FString& N,TArray<TSharedPtr<FJsonValue>>,TArray<TSharedPtr<FJsonValue>>){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto* G=FBlueprintEditorUtils::CreateNewGraph(B,*N,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(B,G,false,nullptr);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("created"));R->SetStringField(TEXT("name"),N);return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveFunction(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	for(UEdGraph* G:B->FunctionGraphs)if(G->GetName()==N){FBlueprintEditorUtils::RemoveGraph(B,G);FBlueprintEditorUtils::MarkBlueprintAsModified(B);auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;}
	ERR("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListGraphs(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(UEdGraph* G:B->UbergraphPages){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("type"),TEXT("EventGraph"));J->SetNumberField(TEXT("nodes"),G->Nodes.Num());A.Add(JOV(J));}
	for(UEdGraph* G:B->FunctionGraphs){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("type"),TEXT("Function"));J->SetNumberField(TEXT("nodes"),G->Nodes.Num());A.Add(JOV(J));}
	for(UEdGraph* G:B->MacroGraphs){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("type"),TEXT("Macro"));J->SetNumberField(TEXT("nodes"),G->Nodes.Num());A.Add(JOV(J));}
	R->SetArrayField(TEXT("graphs"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListComponents(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	if(B->SimpleConstructionScript)for(auto* N:B->SimpleConstructionScript->GetAllNodes()){auto J=JO();J->SetStringField(TEXT("name"),N->GetVariableName().ToString());J->SetStringField(TEXT("class"),N->ComponentClass?N->ComponentClass->GetName():TEXT("None"));A.Add(JOV(J));}
	R->SetArrayField(TEXT("components"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::AddComponent(const FString& P,const FString& CC,const FString& VN){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	UClass* Cls=FindObject<UClass>(nullptr,*CC);if(!Cls){Cls=FindObject<UClass>(nullptr,*(FString(TEXT("/Script/Engine."))+CC));}
	if(!Cls||!Cls->IsChildOf(UActorComponent::StaticClass()))ERR(FString::Printf(TEXT("Component not found: %s"),*CC));
	if(!B->SimpleConstructionScript){B->SimpleConstructionScript=NewObject<USimpleConstructionScript>(B);B->SimpleConstructionScript->SetFlags(RF_Transactional);}
	auto* Node=B->SimpleConstructionScript->CreateNode(Cls,FName(*VN));if(!Node)ERR("CreateNode failed");
	B->SimpleConstructionScript->AddNode(Node);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("added"));R->SetStringField(TEXT("name"),VN);return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveComponent(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");if(!B->SimpleConstructionScript)ERR("No SCS");
	for(USCS_Node* Nd:B->SimpleConstructionScript->GetAllNodes())if(Nd->GetVariableName()==*N){B->SimpleConstructionScript->RemoveNode(Nd);FBlueprintEditorUtils::MarkBlueprintAsModified(B);auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;}
	ERR("Not found");
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListInterfaces(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(const auto& I:B->ImplementedInterfaces){auto J=JO();J->SetStringField(TEXT("class"),I.Interface?I.Interface->GetName():TEXT("None"));A.Add(JOV(J));}
	R->SetArrayField(TEXT("interfaces"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::AddInterface(const FString& P,const FString& IC){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	UClass* Cls=FindObject<UClass>(nullptr,*IC);if(!Cls||!Cls->IsChildOf<UInterface>())ERR(FString::Printf(TEXT("Interface not found: %s"),*IC));
	FBlueprintEditorUtils::ImplementNewInterface(B,FName(*IC));FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("implemented"));R->SetStringField(TEXT("interface"),IC);return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveInterface(const FString& P,const FString& IC){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	FBlueprintEditorUtils::RemoveInterface(B,FName(*IC));FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::ListMacros(const FString& P){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	auto R=JO();TArray<TSharedPtr<FJsonValue>> A;
	for(UEdGraph* G:B->MacroGraphs){auto J=JO();J->SetStringField(TEXT("name"),G->GetName());J->SetNumberField(TEXT("nodes"),G->Nodes.Num());A.Add(JOV(J));}
	R->SetArrayField(TEXT("macros"),A);R->SetNumberField(TEXT("count"),A.Num());return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::CreateMacro(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(B,*N,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(B,G,false,nullptr);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("created"));R->SetStringField(TEXT("name"),N);return R;
}
TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveMacro(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	for(UEdGraph* G:B->MacroGraphs)if(G->GetName()==N){FBlueprintEditorUtils::RemoveGraph(B,G);FBlueprintEditorUtils::MarkBlueprintAsModified(B);auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;}
	ERR("Not found");
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::AddEventDispatcher(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	FEdGraphPinType PT;PT.PinCategory=UEdGraphSchema_K2::PC_MCDelegate;
	FBlueprintEditorUtils::AddMemberVariable(B,*N,PT);FBlueprintEditorUtils::MarkBlueprintAsModified(B);
	auto R=JO();R->SetStringField(TEXT("status"),TEXT("created"));R->SetStringField(TEXT("name"),N);return R;
}

TSharedPtr<FJsonObject> FUE5AIAUTOBlueprintEditor::RemoveEventDispatcher(const FString& P,const FString& N){
	UBlueprint* B=GetCachedBP(P);if(!B)ERR("Not found");
	for(int32 i=0;i<B->NewVariables.Num();++i)if(B->NewVariables[i].VarName==*N&&B->NewVariables[i].VarType.PinCategory==UEdGraphSchema_K2::PC_MCDelegate)
	{B->NewVariables.RemoveAt(i);FBlueprintEditorUtils::MarkBlueprintAsModified(B);auto R=JO();R->SetStringField(TEXT("status"),TEXT("removed"));return R;}
	ERR("Not found");
}

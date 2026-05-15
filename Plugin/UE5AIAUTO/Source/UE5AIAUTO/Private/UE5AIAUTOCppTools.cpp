// C++ native tools - Input, Physics, Audio, Navigation, Animation Blueprint
#include "UE5AIAUTOCppTools.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/InputSettings.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Animation/AnimBlueprint.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTreeEditorTypes.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeDecoratorGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "Engine/DataTable.h"
#include "JsonObjectConverter.h"

static UBehaviorTreeGraph* _BTG(UBehaviorTree* BT){auto* G=Cast<UBehaviorTreeGraph>(BT->BTGraph);if(!G){G=NewObject<UBehaviorTreeGraph>(BT);BT->BTGraph=G;}return G;}
static UEdGraphNode* _FindBTNode(UBehaviorTreeGraph* G,const FString& Id){for(UEdGraphNode* N:G->Nodes)if(N&&N->NodeGuid.ToString()==Id)return N;return nullptr;}

static TSharedPtr<FJsonObject> MakeJsonObj(){return TSharedPtr<FJsonObject>(new FJsonObject());}
static TSharedPtr<FJsonObject> MakeJsonErr(const FString& M){auto J=MakeJsonObj();J->SetStringField(TEXT("error"),M);return J;}

// === Input ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddActionMapping(const FString& N, const FString& K){
	auto J=MakeJsonObj();UInputSettings::GetInputSettings()->AddActionMapping(FInputActionKeyMapping(FName(*N),FKey(*K)),false);UInputSettings::GetInputSettings()->SaveKeyMappings();J->SetStringField(TEXT("status"),TEXT("added"));return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddAxisMapping(const FString& N, const FString& K, float S){
	auto J=MakeJsonObj();UInputSettings::GetInputSettings()->AddAxisMapping(FInputAxisKeyMapping(FName(*N),FKey(*K),S),false);UInputSettings::GetInputSettings()->SaveKeyMappings();J->SetStringField(TEXT("status"),TEXT("added"));return J;
}

// === Physics ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::EnablePhysics(const FString& An, bool bE){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UPrimitiveComponent*> Cs;(*It)->GetComponents<UPrimitiveComponent>(Cs);for(auto* C:Cs)C->SetSimulatePhysics(bE);auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("ok"));return J;}}
	return MakeJsonErr("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::SetMass(const FString& An, float M){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UPrimitiveComponent*> Cs;(*It)->GetComponents<UPrimitiveComponent>(Cs);for(auto* C:Cs)C->SetMassOverrideInKg(NAME_None,M,true);auto J=MakeJsonObj();J->SetNumberField(TEXT("mass"),M);J->SetStringField(TEXT("status"),TEXT("ok"));return J;}}
	return MakeJsonErr("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::ApplyForce(const FString& An, double X, double Y, double Z){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UPrimitiveComponent*> Cs;(*It)->GetComponents<UPrimitiveComponent>(Cs);for(auto* C:Cs)C->AddForce(FVector(X,Y,Z),NAME_None,true);auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("ok"));return J;}}
	return MakeJsonErr("Not found");
}

// === Audio ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::PlaySound(const FString& An, const FString& SP){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	auto* S=LoadObject<USoundBase>(nullptr,*SP);if(!S)return MakeJsonErr("Sound not found");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){UGameplayStatics::SpawnSoundAttached(S,(*It)->GetRootComponent());auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("playing"));return J;}}
	return MakeJsonErr("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::StopSound(const FString& An){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UAudioComponent*> As;(*It)->GetComponents<UAudioComponent>(As);for(auto* A:As)A->Stop();auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("stopped"));return J;}}
	return MakeJsonErr("Not found");
}

// === Navigation ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::FindPathTo(const FString& An, double X, double Y, double Z){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	UNavigationSystemV1* NS=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);if(!NS)return MakeJsonErr("No NavSystem");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()!=An)continue;
		FPathFindingQuery Q;Q.StartLocation=(*It)->GetActorLocation();Q.EndLocation=FVector(X,Y,Z);Q.NavData=NS->GetDefaultNavDataInstance();
		FPathFindingResult Res=NS->FindPathSync(Q);
		auto J=MakeJsonObj();
		if(Res.IsSuccessful()){TArray<TSharedPtr<FJsonValue>> Pts;for(auto& Pt:Res.Path->GetPathPoints()){auto PJ=TSharedPtr<FJsonObject>(new FJsonObject());PJ->SetNumberField(TEXT("x"),Pt.Location.X);PJ->SetNumberField(TEXT("y"),Pt.Location.Y);PJ->SetNumberField(TEXT("z"),Pt.Location.Z);Pts.Add(MakeShareable(new FJsonValueObject(PJ)));}J->SetArrayField(TEXT("path"),Pts);J->SetNumberField(TEXT("points"),Pts.Num());}
		else{J->SetStringField(TEXT("error"),TEXT("No path found"));}
		return J;
	}
	return MakeJsonErr("Not found");
}

// === Behavior Tree ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateBehaviorTree(const FString& Path,const FString& Name){
	auto* Pkg=CreatePackage(*Path);auto* BT=NewObject<UBehaviorTree>(Pkg,UBehaviorTree::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!BT)return MakeJsonErr("CreateBehaviorTree failed");Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(BT);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("path"),Path);return J;
}
static UBehaviorTree* _LoadBT(const FString& Path){
FString PkgPath=FPaths::GetPath(Path);FString Asset=FPaths::GetBaseFilename(Path);
FString ObjPath=PkgPath+TEXT(".")+Asset;
auto* BT=LoadObject<UBehaviorTree>(nullptr,*ObjPath);if(BT)return BT;
BT=FindObject<UBehaviorTree>(nullptr,*ObjPath);if(BT)return BT;
return LoadObject<UBehaviorTree>(nullptr,*Path);
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTComposite(const FString& Path,const FString& ParentId,const FString& CompositeType){
	auto* BT=_LoadBT(Path);if(!BT)return MakeJsonErr(FString::Printf(TEXT("BT not found: %s"),*Path));
	UClass* CC=nullptr;
	if(CompositeType==TEXT("Selector"))CC=UBTComposite_Selector::StaticClass();
	else if(CompositeType==TEXT("Sequence"))CC=UBTComposite_Sequence::StaticClass();
	else if(CompositeType==TEXT("SimpleParallel"))CC=UBTComposite_SimpleParallel::StaticClass();
	else return MakeJsonErr(FString::Printf(TEXT("Unknown composite type: %s (use Selector/Sequence/SimpleParallel)"),*CompositeType));
	auto* G=_BTG(BT);
	auto* Node=NewObject<UBehaviorTreeGraphNode_Composite>(G);
	auto* Inner=NewObject<UBTCompositeNode>(Node,CC,NAME_None,RF_Transactional);
	Node->NodeInstance=Inner;Inner->InitializeFromAsset(*BT);Node->CreateNewGuid();G->AddNode(Node,false,false);
	if(ParentId.IsEmpty()){
		BT->RootNode=Inner;
	}else{
		auto* PN=_FindBTNode(G,ParentId);if(!PN)return MakeJsonErr(FString::Printf(TEXT("Parent not found: %s"),*ParentId));
		auto* PC=Cast<UBehaviorTreeGraphNode_Composite>(PN);if(!PC)return MakeJsonErr("Parent must be a composite node");
		if(UBTCompositeNode* CN=Cast<UBTCompositeNode>(PC->NodeInstance.Get())){FBTCompositeChild Ch;Ch.ChildComposite=Inner;CN->Children.Add(Ch);}
		else return MakeJsonErr("Parent NodeInstance is not a UBTCompositeNode");
	}
	BT->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTTask(const FString& Path,const FString& ParentId,const FString& TaskClassName){
	auto* BT=_LoadBT(Path);if(!BT)return MakeJsonErr(FString::Printf(TEXT("BT not found: %s"),*Path));
	auto* G=_BTG(BT);
	auto* PN=_FindBTNode(G,ParentId);if(!PN)return MakeJsonErr(FString::Printf(TEXT("Parent not found: %s"),*ParentId));
	auto* PC=Cast<UBehaviorTreeGraphNode_Composite>(PN);if(!PC)return MakeJsonErr("Parent must be a composite node");
	UClass* TC=nullptr;
	for(TObjectIterator<UClass> It;It;++It)if(It->IsChildOf(UBTTaskNode::StaticClass())&&It->GetName()==TaskClassName){TC=*It;break;}
	if(!TC)return MakeJsonErr(FString::Printf(TEXT("Task class not found: %s"),*TaskClassName));
	auto* Node=NewObject<UBehaviorTreeGraphNode_Task>(G);
	auto* Inner=NewObject<UBTTaskNode>(Node,TC,NAME_None,RF_Transactional);
	Node->NodeInstance=Inner;Inner->InitializeFromAsset(*BT);Node->CreateNewGuid();G->AddNode(Node,false,false);
	if(UBTCompositeNode* CN=Cast<UBTCompositeNode>(PC->NodeInstance.Get())){FBTCompositeChild Ch;Ch.ChildTask=Inner;CN->Children.Add(Ch);}
	else return MakeJsonErr("Parent NodeInstance is not a UBTCompositeNode");
	BT->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTDecorator(const FString& Path,const FString& NodeId,const FString& DecoratorClassName){
	auto* BT=_LoadBT(Path);if(!BT)return MakeJsonErr(FString::Printf(TEXT("BT not found: %s"),*Path));
	auto* G=_BTG(BT);
	auto* TN=_FindBTNode(G,NodeId);if(!TN)return MakeJsonErr(FString::Printf(TEXT("Node not found: %s"),*NodeId));
	auto* BTGN=Cast<UBehaviorTreeGraphNode>(TN);if(!BTGN)return MakeJsonErr("Target is not a BT graph node");
	if(!BTGN->NodeInstance)return MakeJsonErr("Target node has no BT node instance");
	UClass* DC=nullptr;
	for(TObjectIterator<UClass> It;It;++It)if(It->IsChildOf(UBTDecorator::StaticClass())&&It->GetName()==DecoratorClassName){DC=*It;break;}
	if(!DC)return MakeJsonErr(FString::Printf(TEXT("Decorator class not found: %s"),*DecoratorClassName));
	auto* Node=NewObject<UBehaviorTreeDecoratorGraphNode_Decorator>(TN);
	auto* Inner=NewObject<UBTDecorator>(Node,DC,NAME_None,RF_Transactional);
	Node->NodeInstance=Inner;Inner->InitializeFromAsset(*BT);Node->CreateNewGuid();
	BT->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTService(const FString& Path,const FString& CompositeId,const FString& ServiceClassName){
	auto* BT=_LoadBT(Path);if(!BT)return MakeJsonErr(FString::Printf(TEXT("BT not found: %s"),*Path));
	auto* G=_BTG(BT);
	auto* TN=_FindBTNode(G,CompositeId);if(!TN)return MakeJsonErr(FString::Printf(TEXT("Composite not found: %s"),*CompositeId));
	auto* PC=Cast<UBehaviorTreeGraphNode_Composite>(TN);if(!PC)return MakeJsonErr("Target must be a composite node");
	if(!PC->NodeInstance)return MakeJsonErr("Composite has no BT node instance");
	UClass* SC=nullptr;
	for(TObjectIterator<UClass> It;It;++It)if(It->IsChildOf(UBTService::StaticClass())&&It->GetName()==ServiceClassName){SC=*It;break;}
	if(!SC)return MakeJsonErr(FString::Printf(TEXT("Service class not found: %s"),*ServiceClassName));
	auto* Node=NewObject<UBehaviorTreeGraphNode_Service>(TN);
	auto* Inner=NewObject<UBTService>(Node,SC,NAME_None,RF_Transactional);
	Node->NodeInstance=Inner;Inner->InitializeFromAsset(*BT);Node->CreateNewGuid();
	BT->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());return J;
}

TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateBlackboard(const FString& Path,const FString& Name){
	auto* Pkg=CreatePackage(*Path);auto* BB=NewObject<UBlackboardData>(Pkg,UBlackboardData::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!BB)return MakeJsonErr("CreateBlackboard failed");Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(BB);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("path"),Path);return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBlackboardKey(const FString& Path,const FString& KeyName,const FString& KeyType){
	FString PkgPath=FPaths::GetPath(Path);FString Asset=FPaths::GetBaseFilename(Path);
	FString ObjPath=PkgPath+TEXT(".")+Asset;
	auto* BB=LoadObject<UBlackboardData>(nullptr,*ObjPath);if(!BB)BB=FindObject<UBlackboardData>(nullptr,*ObjPath);if(!BB)BB=LoadObject<UBlackboardData>(nullptr,*Path);if(!BB)return MakeJsonErr(FString::Printf(TEXT("Blackboard not found: %s"),*Path));
	UBlackboardKeyType* KTC=nullptr;
	if(KeyType==TEXT("Bool"))KTC=NewObject<UBlackboardKeyType_Bool>(BB);
	else if(KeyType==TEXT("Int"))KTC=NewObject<UBlackboardKeyType_Int>(BB);
	else if(KeyType==TEXT("Float"))KTC=NewObject<UBlackboardKeyType_Float>(BB);
	else if(KeyType==TEXT("String"))KTC=NewObject<UBlackboardKeyType_String>(BB);
	else if(KeyType==TEXT("Name"))KTC=NewObject<UBlackboardKeyType_Name>(BB);
	else if(KeyType==TEXT("Vector"))KTC=NewObject<UBlackboardKeyType_Vector>(BB);
	else if(KeyType==TEXT("Rotator"))KTC=NewObject<UBlackboardKeyType_Rotator>(BB);
	else if(KeyType==TEXT("Object"))KTC=NewObject<UBlackboardKeyType_Object>(BB);
	else if(KeyType==TEXT("Class"))KTC=NewObject<UBlackboardKeyType_Class>(BB);
	else if(KeyType==TEXT("Enum"))KTC=NewObject<UBlackboardKeyType_Enum>(BB);
	else return MakeJsonErr(FString::Printf(TEXT("Unknown key type: %s"),*KeyType));
	FBlackboardEntry Entry;Entry.EntryName=FName(*KeyName);Entry.KeyType=KTC;
	BB->Keys.Add(Entry);BB->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("added"));J->SetStringField(TEXT("key_name"),KeyName);J->SetStringField(TEXT("key_type"),KeyType);return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateWidgetBlueprint(const FString& Path,const FString& Name){
	FString F=FPaths::GetPath(Path);
	UPackage* Pkg=CreatePackage(*(F/Name));if(!Pkg)return MakeJsonErr("CreatePackage failed");
	UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UUserWidget::StaticClass(),Pkg,FName(*Name),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass());
	if(!BP)return MakeJsonErr("CreateWidgetBlueprint failed");
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(BP);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("path"),F/Name);return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddWidgetToCanvas(const FString& Path,const FString& WidgetClass,const FString& Name){
	UWidgetBlueprint* WBP=LoadObject<UWidgetBlueprint>(nullptr,*Path);
	if(!WBP)return MakeJsonErr(FString::Printf(TEXT("WidgetBlueprint not found: %s"),*Path));
	UWidgetTree* Tree=WBP->WidgetTree;if(!Tree)return MakeJsonErr("No WidgetTree");
	UCanvasPanel* CP=Cast<UCanvasPanel>(Tree->RootWidget);
	if(!CP)return MakeJsonErr("CanvasPanel not found in WidgetBlueprint");
	UClass* WC=nullptr;
	if(WidgetClass==TEXT("Button"))WC=UButton::StaticClass();
	else if(WidgetClass==TEXT("TextBlock"))WC=UTextBlock::StaticClass();
	else if(WidgetClass==TEXT("Image"))WC=UImage::StaticClass();
	else if(WidgetClass==TEXT("CanvasPanel"))WC=UCanvasPanel::StaticClass();
	else if(WidgetClass==TEXT("HorizontalBox"))WC=UHorizontalBox::StaticClass();
	else if(WidgetClass==TEXT("VerticalBox"))WC=UVerticalBox::StaticClass();
	else if(WidgetClass==TEXT("SizeBox"))WC=USizeBox::StaticClass();
	else if(WidgetClass==TEXT("Border"))WC=UBorder::StaticClass();
	if(!WC)return MakeJsonErr(FString::Printf(TEXT("Unknown widget class: %s"),*WidgetClass));
	UWidget* NW=Tree->ConstructWidget<UWidget>(WC,FName(*Name));
	if(!NW)return MakeJsonErr("ConstructWidget failed");
	static constexpr float DefaultWidgetX=100.0f,DefaultWidgetY=100.0f;
	static constexpr float DefaultWidgetW=200.0f,DefaultWidgetH=50.0f;
	UPanelSlot* Slot=CP->AddChild(NW);
	if(UCanvasPanelSlot* CS=Cast<UCanvasPanelSlot>(Slot)){CS->SetPosition(FVector2D(DefaultWidgetX,DefaultWidgetY));CS->SetSize(FVector2D(DefaultWidgetW,DefaultWidgetH));}
	WBP->Modify();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("added"));J->SetStringField(TEXT("widget"),Name);return J;
}
// === Animation Blueprint ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddAnimState(const FString& Path,const FString& StateName,double X,double Y){
	auto* AnimBP=LoadObject<UAnimBlueprint>(nullptr,*Path);if(!AnimBP)return MakeJsonErr("AnimBP not found");
	UAnimationStateMachineGraph* SMG=nullptr;for(UEdGraph* G:AnimBP->UbergraphPages){SMG=Cast<UAnimationStateMachineGraph>(G);if(SMG)break;}
	if(!SMG)return MakeJsonErr("AnimGraph not found — open AnimBP and create one first");
	UAnimStateNode* SN=NewObject<UAnimStateNode>(SMG);if(!SN)return MakeJsonErr("Failed to create state node");
	SN->CreateNewGuid();SN->PostPlacedNewNode();SN->AllocateDefaultPins();SN->NodePosX=X;SN->NodePosY=Y;
	if(SN->BoundGraph)SN->BoundGraph->Rename(*StateName);
	SMG->AddNode(SN,true,false);AnimBP->Modify();FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),SN->NodeGuid.ToString());J->SetStringField(TEXT("state_name"),SN->GetStateName());return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddAnimTransition(const FString& Path,const FString& FromState,const FString& ToState,const FString& Condition){
	auto* AnimBP=LoadObject<UAnimBlueprint>(nullptr,*Path);if(!AnimBP)return MakeJsonErr("AnimBP not found");
	UAnimationStateMachineGraph* SMG=nullptr;for(UEdGraph* G:AnimBP->UbergraphPages){SMG=Cast<UAnimationStateMachineGraph>(G);if(SMG)break;}
	if(!SMG)return MakeJsonErr("AnimGraph not found");
	UAnimStateNode* FromNode=nullptr;UAnimStateNode* ToNode=nullptr;
	for(UEdGraphNode* N:SMG->Nodes){if(UAnimStateNode* S=Cast<UAnimStateNode>(N)){if(S->GetStateName()==FromState)FromNode=S;if(S->GetStateName()==ToState)ToNode=S;}}
	if(!FromNode)return MakeJsonErr(FString::Printf(TEXT("FromState not found: %s"),*FromState));
	if(!ToNode)return MakeJsonErr(FString::Printf(TEXT("ToState not found: %s"),*ToState));
	UAnimStateTransitionNode* TN=NewObject<UAnimStateTransitionNode>(SMG);if(!TN)return MakeJsonErr("Failed to create transition node");
	TN->CreateNewGuid();TN->PostPlacedNewNode();TN->AllocateDefaultPins();
	SMG->AddNode(TN,true,false);TN->CreateConnections(FromNode,ToNode);
	AnimBP->Modify();FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),TN->NodeGuid.ToString());J->SetStringField(TEXT("from"),FromState);J->SetStringField(TEXT("to"),ToState);return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::SetAnimGraphNode(const FString& Path,const FString& NodeClass,double X,double Y){
	auto* AnimBP=LoadObject<UAnimBlueprint>(nullptr,*Path);if(!AnimBP)return MakeJsonErr("AnimBP not found");
	UAnimationStateMachineGraph* SMG=nullptr;for(UEdGraph* G:AnimBP->UbergraphPages){SMG=Cast<UAnimationStateMachineGraph>(G);if(SMG)break;}
	if(!SMG)return MakeJsonErr("AnimGraph not found");
	UClass* NC=FindObject<UClass>(nullptr,*(TEXT("/Script/AnimGraph.")+NodeClass));
	if(!NC)for(TObjectIterator<UClass> It;It;++It)if(It->IsChildOf(UEdGraphNode::StaticClass())&&!(It->ClassFlags&CLASS_Abstract)&&It->GetName()==NodeClass){NC=*It;break;}
	if(!NC)return MakeJsonErr(FString::Printf(TEXT("Node class not found: %s"),*NodeClass));
	UEdGraphNode* Node=NewObject<UEdGraphNode>(SMG,NC);if(!Node)return MakeJsonErr("Failed to create node");
	Node->CreateNewGuid();Node->NodePosX=X;Node->NodePosY=Y;SMG->AddNode(Node,true,false);Node->PostPlacedNewNode();Node->AllocateDefaultPins();
	AnimBP->Modify();FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());J->SetStringField(TEXT("node_class"),Node->GetClass()->GetName());return J;
}

// === DataTable ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateDataTable(const FString& Path,const FString& Name,const FString& RowStructPath){
	UScriptStruct* RowStruct=FindObject<UScriptStruct>(nullptr,*RowStructPath);
	if(!RowStruct)RowStruct=LoadObject<UScriptStruct>(nullptr,*RowStructPath);
	if(!RowStruct){FString P0=TEXT("/Script/CoreUObject.")+RowStructPath;RowStruct=FindObject<UScriptStruct>(nullptr,*P0);}
	if(!RowStruct){FString P1=TEXT("/Script/Engine.")+RowStructPath;RowStruct=FindObject<UScriptStruct>(nullptr,*P1);}
	if(!RowStruct){RowStruct=LoadObject<UScriptStruct>(nullptr,*(TEXT("/Script/Engine.")+RowStructPath));}
	if(!RowStruct)return MakeJsonErr(FString::Printf(TEXT("RowStruct not found: %s (try /Script/Engine.Tablename or /Script/CoreUObject.TypeName)"),*RowStructPath));
	UPackage* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	if(!Pkg)return MakeJsonErr("CreatePackage failed");
	UDataTable* DT=NewObject<UDataTable>(Pkg,UDataTable::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!DT)return MakeJsonErr("NewObject failed");
	DT->RowStruct=RowStruct;
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(DT);
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("path"),FPaths::GetPath(Path)/Name);return J;
}

TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddDataTableRow(const FString& Path,const FString& RowName,const TSharedPtr<FJsonObject>& RowData){
	FString PkgPath=FPaths::GetPath(Path);FString Asset=FPaths::GetBaseFilename(Path);
	FString ObjPath=PkgPath+TEXT(".")+Asset;
	UDataTable* DT=LoadObject<UDataTable>(nullptr,*ObjPath);if(!DT)DT=FindObject<UDataTable>(nullptr,*ObjPath);if(!DT)DT=LoadObject<UDataTable>(nullptr,*Path);
	if(!DT)return MakeJsonErr(FString::Printf(TEXT("DataTable not found: %s"),*Path));
	if(!RowData.IsValid())return MakeJsonErr("RowData is null");
	UScriptStruct* RowStruct=DT->RowStruct;
	if(!RowStruct)return MakeJsonErr("DataTable has no RowStruct set");
	uint8* ExistingRow=DT->FindRowUnchecked(FName(*RowName));
	if(!ExistingRow){
		// RAII: wrap malloc in a struct that auto-frees on exception
		struct FRowGuard { uint8* Ptr; UScriptStruct* SS; ~FRowGuard(){ if(Ptr){ SS->DestroyStruct(Ptr); FMemory::Free(Ptr); } } };
		FRowGuard Guard{nullptr, RowStruct};
		Guard.Ptr=(uint8*)FMemory::Malloc(RowStruct->GetStructureSize());
		RowStruct->InitializeStruct(Guard.Ptr);
		FJsonObjectConverter::JsonObjectToUStruct(RowData.ToSharedRef(),RowStruct,Guard.Ptr,0,0);
		DT->AddRow(FName(*RowName),*reinterpret_cast<FTableRowBase*>(Guard.Ptr));
		Guard.Ptr=nullptr; // ownership transferred to DataTable
	}else{
		FJsonObjectConverter::JsonObjectToUStruct(RowData.ToSharedRef(),RowStruct,ExistingRow,0,0);
	}
	DT->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("added"));J->SetStringField(TEXT("row_name"),RowName);return J;
}

TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::GetDataTableRow(const FString& Path,const FString& RowName){
	FString PkgPath=FPaths::GetPath(Path);FString Asset=FPaths::GetBaseFilename(Path);
	FString ObjPath=PkgPath+TEXT(".")+Asset;
	UDataTable* DT=LoadObject<UDataTable>(nullptr,*ObjPath);if(!DT)DT=FindObject<UDataTable>(nullptr,*ObjPath);if(!DT)DT=LoadObject<UDataTable>(nullptr,*Path);
	if(!DT)return MakeJsonErr(FString::Printf(TEXT("DataTable not found: %s"),*Path));
	UScriptStruct* RowStruct=DT->RowStruct;
	if(!RowStruct)return MakeJsonErr("DataTable has no RowStruct set");
	uint8* Row=DT->FindRowUnchecked(FName(*RowName));
	if(!Row)return MakeJsonErr(FString::Printf(TEXT("Row not found: %s"),*RowName));
	TSharedPtr<FJsonObject> RowObj=MakeShareable(new FJsonObject());
	FJsonObjectConverter::UStructToJsonObject(RowStruct,Row,RowObj.ToSharedRef(),0,0);
	auto J=MakeJsonObj();J->SetObjectField(TEXT("row_data"),RowObj);J->SetStringField(TEXT("row_name"),RowName);return J;
}

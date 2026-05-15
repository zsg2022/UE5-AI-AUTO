// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTOEditorSubsystem.h"
#include "UE5AIAUTOCommandExecutor.h"
#include "UE5AIAUTOReflectionScanner.h"
#include "UE5AIAUTOWebSocketClient.h"
#include "UE5AIAUTOMaterialEditor.h"
#include "UE5AIAUTOBlueprintEditor.h"
#include "UE5AIAUTOCppTools.h"
#include "UE5AIAUTOAdvancedTools.h"
#include "UE5AIAUTOScreenshotCapture.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

// Lifecycle

void UUE5AIAUTOEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] EditorSubsystem Initialized"));

	FString Host = DEFAULT_HOST;
	int32 Port = DEFAULT_PORT;
	GConfig->GetString(TEXT("/Script/UE5AIAUTO.UE5AIAUTOSettings"), TEXT("BridgeHost"), Host, GEditorIni);
	GConfig->GetInt(TEXT("/Script/UE5AIAUTO.UE5AIAUTOSettings"), TEXT("BridgePort"), Port, GEditorIni);
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Bridge config: %s:%d"), *Host, Port);

	CommandExecutor = MakeUnique<FUE5AIAUTOCommandExecutor>();
	RegisterCommandHandlers();

	WebSocketClient = MakeUnique<FUE5AIAUTOWebSocketClient>(Host, Port);
	WebSocketClient->OnConnected.AddLambda([this]()
	{
		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Connected to MCP Server"));
	});
	WebSocketClient->OnDisconnected.AddLambda([](const FString& Reason)
	{
		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Disconnected: %s"), *Reason);
	});
	WebSocketClient->OnResponse.AddLambda([this](const FGuid& Id, const FString& Json)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(R, Obj) || !Obj.IsValid()) return;

		if (!Obj->HasField(TEXT("method"))) return;
			FString Method = Obj->GetStringField(TEXT("method"));
		TSharedPtr<FJsonObject> Params = Obj->GetObjectField(TEXT("params"));
		if (!Params.IsValid()) Params = MakeShareable(new FJsonObject());

		if (!GEditor) { TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject()); Err->SetStringField(TEXT("error"), TEXT("Editor not available")); return; }

		TSharedPtr<FJsonObject> Result = CommandExecutor->ExecuteImmediate(Method, Params);

			FString ResultJson;
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResultJson);
			FJsonSerializer::Serialize(Result.ToSharedRef(), W);
			FString Resp = TEXT("{\"id\":\"") + Id.ToString(EGuidFormats::DigitsWithHyphens) + TEXT("\",\"result\":") + ResultJson + TEXT("}");
		WebSocketClient->SendCommand(Id, Resp);
	});
	WebSocketClient->Connect();

	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UUE5AIAUTOEditorSubsystem::Tick));
}

void UUE5AIAUTOEditorSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] EditorSubsystem Deinitialized"));
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}
	if (WebSocketClient)
	{
		WebSocketClient->Disconnect();
		WebSocketClient.Reset();
	}
	CommandExecutor.Reset();
	Super::Deinitialize();
}

bool UUE5AIAUTOEditorSubsystem::Tick(float DeltaTime)
{
	if (CommandExecutor)
	{
		CommandExecutor->Tick(DeltaTime);
	}
	return true;
}

// Command Handlers

void UUE5AIAUTOEditorSubsystem::RegisterCommandHandlers()
{
	RegisterActorHandlers();
	RegisterBPHandlers();
	RegisterAdvancedHandlers();
	RegisterCppToolsHandlers();
	RegisterMiscHandlers();

	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Handlers registered: %d"),
		CommandExecutor->GetRegisteredMethods().Num());
}

static TSharedPtr<FJsonObject> MakeJsonError(const FString& Msg, EUE5AIAUTOErrorCode Code = EUE5AIAUTOErrorCode::INTERNAL)
{
	TSharedPtr<FJsonObject> E = MakeShareable(new FJsonObject());
	E->SetStringField(TEXT("error"), Msg);
	E->SetNumberField(TEXT("error_code"), static_cast<int32>(Code));
	return E;
}

void UUE5AIAUTOEditorSubsystem::RegisterActorHandlers()
{
	CommandExecutor->RegisterHandler(TEXT("ping"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("status"), TEXT("ok"));
			return Result;
		});

	CommandExecutor->RegisterHandler(TEXT("create_actor"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FString ClassName;
			if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
				return MakeJsonError(TEXT("Missing required param: class_name"), EUE5AIAUTOErrorCode::INVALID_ARG);
			const TSharedPtr<FJsonObject>* LocObj = nullptr;
			FVector Location = FVector::ZeroVector;
			if (Params->TryGetObjectField(TEXT("location"), LocObj))
			{
				Location.X = (*LocObj)->GetNumberField(TEXT("x"));
				Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
				Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
			}

			UClass* Class = UClass::TryFindTypeSlow<UClass>(ClassName);
			if (!Class)
			{
				Class = UClass::TryFindTypeSlow<UClass>(FString(TEXT("A")) + ClassName);
			}
			if (!Class) return MakeJsonError(FString::Printf(TEXT("Class not found: %s"), *ClassName), EUE5AIAUTOErrorCode::NOT_FOUND);

			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (!World) return MakeJsonError(TEXT("World not available"));

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* Actor = World->SpawnActor<AActor>(Class, Location, FRotator::ZeroRotator, SpawnParams);
			if (!Actor) return MakeJsonError(FString::Printf(TEXT("SpawnActor failed: %s"), *ClassName));

			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("actor_name"), Actor->GetName());
			Result->SetStringField(TEXT("class_name"), Actor->GetClass()->GetName());
			TSharedPtr<FJsonObject> LocResult = MakeShareable(new FJsonObject());
			LocResult->SetNumberField(TEXT("x"), Actor->GetActorLocation().X);
			LocResult->SetNumberField(TEXT("y"), Actor->GetActorLocation().Y);
			LocResult->SetNumberField(TEXT("z"), Actor->GetActorLocation().Z);
			Result->SetObjectField(TEXT("location"), LocResult);
			return Result;
		});

	CommandExecutor->RegisterHandler(TEXT("modify_actor_property"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FString ActorName, PropName;
			if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
				return MakeJsonError(TEXT("Missing required param: actor_name"), EUE5AIAUTOErrorCode::INVALID_ARG);
			if (!Params->TryGetStringField(TEXT("property_name"), PropName))
				return MakeJsonError(TEXT("Missing required param: property_name"), EUE5AIAUTOErrorCode::INVALID_ARG);
			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (!World) return MakeJsonError(TEXT("World not available"));

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (Actor->GetName() != ActorName) continue;

				FProperty* Prop = Actor->GetClass()->FindPropertyByName(*PropName);
				if (!Prop) return MakeJsonError(FString::Printf(TEXT("Property not found: %s::%s"), *ActorName, *PropName));

				TSharedPtr<FJsonValue> ValPtr = Params->TryGetField(TEXT("value"));
				if (ValPtr.IsValid())
				{
					if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
						BP->SetPropertyValue_InContainer(Actor, ValPtr->AsBool());
					else if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
						FP->SetPropertyValue_InContainer(Actor, ValPtr->AsNumber());
					else if (FIntProperty* IP = CastField<FIntProperty>(Prop))
						IP->SetPropertyValue_InContainer(Actor, (int32)ValPtr->AsNumber());
					else if (FStrProperty* SP = CastField<FStrProperty>(Prop))
						SP->SetPropertyValue_InContainer(Actor, ValPtr->AsString());
				}

				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
				Result->SetStringField(TEXT("actor_name"), ActorName);
				Result->SetStringField(TEXT("property"), PropName);
				Result->SetStringField(TEXT("status"), TEXT("modified"));
				return Result;
			}
			return MakeJsonError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		});

	CommandExecutor->RegisterHandler(TEXT("delete_actor"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FString ActorName;
			if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
				return MakeJsonError(TEXT("Missing required param: actor_name"), EUE5AIAUTOErrorCode::INVALID_ARG);
			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (!World) return MakeJsonError(TEXT("World not available"));
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if ((*It)->GetName() == ActorName)
				{
					World->DestroyActor(*It);
					TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
					Result->SetStringField(TEXT("actor_name"), ActorName);
					Result->SetStringField(TEXT("status"), TEXT("deleted"));
					return Result;
				}
			}
			return MakeJsonError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		});

	CommandExecutor->RegisterHandler(TEXT("query_scene"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			UWorld* World = GEditor->GetEditorWorldContext().World();
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			TArray<TSharedPtr<FJsonValue>> ActorArray;
			if (World)
			{
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					AActor* Actor = *It;
					TSharedPtr<FJsonObject> ActorObj = MakeShareable(new FJsonObject());
					ActorObj->SetStringField(TEXT("name"), Actor->GetName());
					ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
					TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject());
					LocObj->SetNumberField(TEXT("x"), Actor->GetActorLocation().X);
					LocObj->SetNumberField(TEXT("y"), Actor->GetActorLocation().Y);
					LocObj->SetNumberField(TEXT("z"), Actor->GetActorLocation().Z);
					ActorObj->SetObjectField(TEXT("location"), LocObj);
					ActorArray.Add(MakeShareable(new FJsonValueObject(ActorObj)));
				}
			}
			Result->SetArrayField(TEXT("actors"), ActorArray);
			Result->SetNumberField(TEXT("count"), ActorArray.Num());
			return Result;
		});

	CommandExecutor->RegisterHandler(TEXT("set_viewport_camera"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			const TSharedPtr<FJsonObject>* LocObj = nullptr;
			if (!Params->TryGetObjectField(TEXT("location"), LocObj))
				return MakeJsonError(TEXT("Missing location"));
			FVector Loc((*LocObj)->GetNumberField(TEXT("x")),
				(*LocObj)->GetNumberField(TEXT("y")),
				(*LocObj)->GetNumberField(TEXT("z")));

			FViewport* VP = GEditor->GetActiveViewport();
			if (VP)
			{
				FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(VP->GetClient());
				if (Client) Client->SetViewLocation(Loc);
			}
			// Rotation support
			const TSharedPtr<FJsonObject>* RotObj = nullptr;
			if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
			{
				FRotator Rot((*RotObj)->GetNumberField(TEXT("pitch")),
					(*RotObj)->GetNumberField(TEXT("yaw")),
					(*RotObj)->GetNumberField(TEXT("roll")));
				if (VP)
				{
					FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(VP->GetClient());
					if (Client) Client->SetViewRotation(Rot);
				}
			}
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("status"), TEXT("ok"));
			return Result;
		});
}

void UUE5AIAUTOEditorSubsystem::RegisterBPHandlers()
{
	// NOTE: BP_REG captures P by reference — handlers MUST be synchronous (ExecuteImmediate).
	// Do not enqueue these handlers to async queue; P would dangle.
#define BP_REG(NAME, Fn) CommandExecutor->RegisterHandler(TEXT(NAME), \
	[](TSharedPtr<FJsonObject> P)->TSharedPtr<FJsonObject>{ return Fn; })

	BP_REG("bp_list", FUE5AIAUTOBlueprintEditor::ListBlueprints(P->GetStringField(TEXT("path"))));
	BP_REG("bp_open", FUE5AIAUTOBlueprintEditor::OpenBlueprint(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create", FUE5AIAUTOBlueprintEditor::CreateBlueprint(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("parent_class"))));
	BP_REG("bp_compile", FUE5AIAUTOBlueprintEditor::CompileBlueprint(P->GetStringField(TEXT("path"))));
	BP_REG("bp_list_nodes", FUE5AIAUTOBlueprintEditor::ListNodes(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph"))));
	BP_REG("bp_create_node",
		FUE5AIAUTOBlueprintEditor::CreateNode(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")),
			P->GetStringField(TEXT("node_class")), P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y")),
			P->GetObjectField(TEXT("defaults"))));
	BP_REG("bp_remove_node", FUE5AIAUTOBlueprintEditor::RemoveNode(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("node_id"))));
	BP_REG("bp_set_pin_default", FUE5AIAUTOBlueprintEditor::SetPinDefault(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("node_id")), P->GetStringField(TEXT("pin")), P->GetStringField(TEXT("value"))));
	BP_REG("bp_connect_pins", FUE5AIAUTOBlueprintEditor::ConnectPins(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("src_node")), P->GetStringField(TEXT("src_pin")), P->GetStringField(TEXT("dst_node")), P->GetStringField(TEXT("dst_pin"))));
	BP_REG("bp_disconnect_pin", FUE5AIAUTOBlueprintEditor::DisconnectPin(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("node_id")), P->GetStringField(TEXT("pin"))));
	BP_REG("bp_list_variables", FUE5AIAUTOBlueprintEditor::ListVariables(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create_variable", FUE5AIAUTOBlueprintEditor::CreateVariable(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name")), P->GetStringField(TEXT("type")), P->GetBoolField(TEXT("is_array"))));
	BP_REG("bp_remove_variable", FUE5AIAUTOBlueprintEditor::RemoveVariable(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_set_variable_default", FUE5AIAUTOBlueprintEditor::SetVariableDefault(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name")), P->GetStringField(TEXT("value"))));
	BP_REG("bp_list_functions", FUE5AIAUTOBlueprintEditor::ListFunctions(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create_function", FUE5AIAUTOBlueprintEditor::CreateFunction(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name")), P->GetArrayField(TEXT("inputs")), P->GetArrayField(TEXT("outputs"))));
	BP_REG("bp_remove_function", FUE5AIAUTOBlueprintEditor::RemoveFunction(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_list_graphs", FUE5AIAUTOBlueprintEditor::ListGraphs(P->GetStringField(TEXT("path"))));
	BP_REG("bp_list_node_classes", FUE5AIAUTOBlueprintEditor::ListNodeClasses());
	BP_REG("bp_list_components", FUE5AIAUTOBlueprintEditor::ListComponents(P->GetStringField(TEXT("path"))));
	BP_REG("bp_add_component", FUE5AIAUTOBlueprintEditor::AddComponent(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("component_class")), P->GetStringField(TEXT("variable_name"))));
	BP_REG("bp_remove_component", FUE5AIAUTOBlueprintEditor::RemoveComponent(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_list_interfaces", FUE5AIAUTOBlueprintEditor::ListInterfaces(P->GetStringField(TEXT("path"))));
	BP_REG("bp_add_interface", FUE5AIAUTOBlueprintEditor::AddInterface(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("interface_class"))));
	BP_REG("bp_remove_interface", FUE5AIAUTOBlueprintEditor::RemoveInterface(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("interface_class"))));
	BP_REG("bp_list_macros", FUE5AIAUTOBlueprintEditor::ListMacros(P->GetStringField(TEXT("path"))));
	BP_REG("bp_add_event_dispatcher", FUE5AIAUTOBlueprintEditor::AddEventDispatcher(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_save", FUE5AIAUTOBlueprintEditor::SaveBlueprint(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create_macro", FUE5AIAUTOBlueprintEditor::CreateMacro(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_remove_macro", FUE5AIAUTOBlueprintEditor::RemoveMacro(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_remove_event_dispatcher", FUE5AIAUTOBlueprintEditor::RemoveEventDispatcher(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));

#undef BP_REG
}

void UUE5AIAUTOEditorSubsystem::RegisterAdvancedHandlers()
{
	CommandExecutor->RegisterHandler(TEXT("create_material"),
		[](TSharedPtr<FJsonObject> P) -> TSharedPtr<FJsonObject>
		{
			FMaterialCreateParams M;
			M.AssetPath = P->GetStringField(TEXT("asset_path"));
			if (M.AssetPath.IsEmpty()) return MakeJsonError(TEXT("asset_path is required"));
			P->TryGetStringField(TEXT("material_type"), M.MaterialType);
			P->TryGetStringField(TEXT("base_color_hex"), M.BaseColorHex);
			P->TryGetNumberField(TEXT("opacity"), M.Opacity);
			P->TryGetNumberField(TEXT("roughness"), M.Roughness);
			P->TryGetNumberField(TEXT("metallic"), M.Metallic);
			P->TryGetNumberField(TEXT("specular"), M.Specular);
			P->TryGetNumberField(TEXT("emissive_strength"), M.EmissiveStrength);
			P->TryGetStringField(TEXT("emissive_hex"), M.EmissiveHex);
			return FUE5AIAUTOMaterialEditor::CreatePbrMaterial(M);
		});

	CommandExecutor->RegisterHandler(TEXT("create_metasound"), [](auto P){return FUE5AIAUTOAdvancedTools::CreateMetaSoundSource(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("create_niagara"), [](auto P){return FUE5AIAUTOAdvancedTools::CreateNiagaraSystem(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_emitter"), [](auto P){return FUE5AIAUTOAdvancedTools::AddNiagaraEmitter(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("emitter_path")));});
	CommandExecutor->RegisterHandler(TEXT("set_niagara_param"), [](auto P){return FUE5AIAUTOAdvancedTools::SetNiagaraParameter(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("param_name")),P->GetNumberField(TEXT("value")));});
	CommandExecutor->RegisterHandler(TEXT("create_sequence"), [](auto P){return FUE5AIAUTOAdvancedTools::CreateLevelSequence(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_actor_to_sequencer"), [](auto P){return FUE5AIAUTOAdvancedTools::AddActorToSequencer(P->GetStringField(TEXT("seq_path")),P->GetStringField(TEXT("actor_name")));});
	CommandExecutor->RegisterHandler(TEXT("add_transform_track"), [](auto P){return FUE5AIAUTOAdvancedTools::AddTransformTrack(P->GetStringField(TEXT("seq_path")),P->GetStringField(TEXT("actor_name")));});
	CommandExecutor->RegisterHandler(TEXT("set_keyframe"), [](auto P){return FUE5AIAUTOAdvancedTools::SetKeyframe(P->GetStringField(TEXT("seq_path")),P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("time")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")),P->GetNumberField(TEXT("z")));});
	// PCG disabled - plugin not available in project
	// CommandExecutor->RegisterHandler(TEXT("create_pcg"), [](auto P){return FUE5AIAUTOAdvancedTools::CreatePCGGraph(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("copy_asset"), [](auto P){return FUE5AIAUTOAdvancedTools::CopyAsset(P->GetStringField(TEXT("source_path")),P->GetStringField(TEXT("dest_path")));});
	CommandExecutor->RegisterHandler(TEXT("delete_asset"), [](auto P){return FUE5AIAUTOAdvancedTools::DeleteAsset(P->GetStringField(TEXT("path")));});
	CommandExecutor->RegisterHandler(TEXT("list_assets"), [](auto P){return FUE5AIAUTOAdvancedTools::ListAssets(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("class_name")));});
}

void UUE5AIAUTOEditorSubsystem::RegisterCppToolsHandlers()
{
	CommandExecutor->RegisterHandler(TEXT("bt_create"), [](auto P){return FUE5AIAUTOCppTools::CreateBehaviorTree(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("create_blackboard"), [](auto P){return FUE5AIAUTOCppTools::CreateBlackboard(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_bb_key"), [](auto P){return FUE5AIAUTOCppTools::AddBlackboardKey(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("key_name")),P->GetStringField(TEXT("key_type")));});
	CommandExecutor->RegisterHandler(TEXT("bt_add_composite"), [](auto P){return FUE5AIAUTOCppTools::AddBTComposite(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("parent_id")),P->GetStringField(TEXT("composite_type")));});
	CommandExecutor->RegisterHandler(TEXT("bt_add_task"), [](auto P){return FUE5AIAUTOCppTools::AddBTTask(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("parent_id")),P->GetStringField(TEXT("task_class")));});
	CommandExecutor->RegisterHandler(TEXT("bt_add_decorator"), [](auto P){return FUE5AIAUTOCppTools::AddBTDecorator(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("node_id")),P->GetStringField(TEXT("decorator_class")));});
	CommandExecutor->RegisterHandler(TEXT("bt_add_service"), [](auto P){return FUE5AIAUTOCppTools::AddBTService(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("composite_id")),P->GetStringField(TEXT("service_class")));});
	CommandExecutor->RegisterHandler(TEXT("anim_add_state"), [](auto P){return FUE5AIAUTOCppTools::AddAnimState(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("state_name")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")));});
	CommandExecutor->RegisterHandler(TEXT("anim_add_transition"), [](auto P){return FUE5AIAUTOCppTools::AddAnimTransition(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("from_state")),P->GetStringField(TEXT("to_state")),P->GetStringField(TEXT("condition")));});
	CommandExecutor->RegisterHandler(TEXT("anim_set_graph_node"), [](auto P){return FUE5AIAUTOCppTools::SetAnimGraphNode(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("node_class")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")));});
	CommandExecutor->RegisterHandler(TEXT("create_widget"), [](auto P){return FUE5AIAUTOCppTools::CreateWidgetBlueprint(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_widget_to_canvas"), [](auto P){return FUE5AIAUTOCppTools::AddWidgetToCanvas(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("widget_class")),P->GetStringField(TEXT("name")));});
		CommandExecutor->RegisterHandler(TEXT("widget_set_text"), [](auto P){return FUE5AIAUTOCppTools::WidgetSetText(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("widget")),P->GetStringField(TEXT("text")));});
		CommandExecutor->RegisterHandler(TEXT("widget_set_font"), [](auto P){return FUE5AIAUTOCppTools::WidgetSetFont(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("widget")),P->GetNumberField(TEXT("size")),P->GetStringField(TEXT("color")));});
		CommandExecutor->RegisterHandler(TEXT("widget_list_tree"), [](auto P){return FUE5AIAUTOCppTools::WidgetListTree(P->GetStringField(TEXT("path")));});
		CommandExecutor->RegisterHandler(TEXT("widget_set_position"), [](auto P){return FUE5AIAUTOCppTools::WidgetSetPosition(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("widget")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")),P->GetNumberField(TEXT("w")),P->GetNumberField(TEXT("h")));});
		CommandExecutor->RegisterHandler(TEXT("widget_add_to_viewport"), [](auto P){return FUE5AIAUTOCppTools::WidgetAddToViewport(P->GetStringField(TEXT("path")));});
		CommandExecutor->RegisterHandler(TEXT("widget_set_visibility"), [](auto P){return FUE5AIAUTOCppTools::WidgetSetVisibility(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("widget")),P->GetStringField(TEXT("visible")));});
	CommandExecutor->RegisterHandler(TEXT("add_action_mapping"), [](auto P){return FUE5AIAUTOCppTools::AddActionMapping(P->GetStringField(TEXT("action_name")),P->GetStringField(TEXT("key")));});
	CommandExecutor->RegisterHandler(TEXT("add_axis_mapping"), [](auto P){return FUE5AIAUTOCppTools::AddAxisMapping(P->GetStringField(TEXT("axis_name")),P->GetStringField(TEXT("key")),P->GetNumberField(TEXT("scale")));});
	CommandExecutor->RegisterHandler(TEXT("enable_physics"), [](auto P){return FUE5AIAUTOCppTools::EnablePhysics(P->GetStringField(TEXT("actor_name")),P->GetBoolField(TEXT("enable")));});
	CommandExecutor->RegisterHandler(TEXT("set_mass"), [](auto P){return FUE5AIAUTOCppTools::SetMass(P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("mass")));});
	CommandExecutor->RegisterHandler(TEXT("play_sound"), [](auto P){return FUE5AIAUTOCppTools::PlaySound(P->GetStringField(TEXT("actor_name")),P->GetStringField(TEXT("sound_path")));});
	CommandExecutor->RegisterHandler(TEXT("find_path"), [](auto P){return FUE5AIAUTOCppTools::FindPathTo(P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")),P->GetNumberField(TEXT("z")));});
	CommandExecutor->RegisterHandler(TEXT("apply_force"), [](auto P){return FUE5AIAUTOCppTools::ApplyForce(P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")),P->GetNumberField(TEXT("z")));});
	CommandExecutor->RegisterHandler(TEXT("stop_sound"), [](auto P){return FUE5AIAUTOCppTools::StopSound(P->GetStringField(TEXT("actor_name")));});
	CommandExecutor->RegisterHandler(TEXT("create_datatable"), [](auto P){return FUE5AIAUTOCppTools::CreateDataTable(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")),P->GetStringField(TEXT("row_struct")));});
	CommandExecutor->RegisterHandler(TEXT("add_datatable_row"), [](auto P){return FUE5AIAUTOCppTools::AddDataTableRow(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("row_name")),P->GetObjectField(TEXT("row_data")));});
	CommandExecutor->RegisterHandler(TEXT("get_datatable_row"), [](auto P){return FUE5AIAUTOCppTools::GetDataTableRow(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("row_name")));});
}

void UUE5AIAUTOEditorSubsystem::RegisterMiscHandlers()
{
	CommandExecutor->RegisterHandler(TEXT("scan_reflection"),
		[this](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FUE5AIAUTOReflectionScanner::FScanResult ScanResult =
				FUE5AIAUTOReflectionScanner::ScanAllModules();
			const FString CachePath = FPaths::ProjectSavedDir() /
				TEXT("UE5AIAUTO") / TEXT("reflection_cache.json");
			FUE5AIAUTOReflectionScanner::SaveCacheToJson(ScanResult, CachePath);

			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetNumberField(TEXT("function_count"), ScanResult.FunctionCount);
			Result->SetNumberField(TEXT("class_count"), ScanResult.ClassNames.Num());
			Result->SetStringField(TEXT("cache_path"), CachePath);
			return Result;
		});

	CommandExecutor->RegisterHandler(TEXT("screenshot"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FUE5AIAUTOScreenshotCapture::FCaptureSettings Settings;
			Params->TryGetNumberField(TEXT("width"), Settings.Width);
			Params->TryGetNumberField(TEXT("height"), Settings.Height);
			Params->TryGetNumberField(TEXT("quality"), Settings.Quality);
			FString Fmt;
			if (Params->TryGetStringField(TEXT("format"), Fmt))
				Settings.bJPEG = (Fmt != TEXT("png"));

			TArray<uint8> Data = FUE5AIAUTOScreenshotCapture::CaptureViewport(Settings);
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			if (Data.Num() > 0)
			{
				Result->SetStringField(TEXT("status"), TEXT("ok"));
				Result->SetStringField(TEXT("format"), Settings.bJPEG ? TEXT("jpeg") : TEXT("png"));
				Result->SetNumberField(TEXT("size_bytes"), Data.Num());
				Result->SetStringField(TEXT("data_base64"), FBase64::Encode(Data));
			}
			else
			{
				Result->SetStringField(TEXT("error"), TEXT("Screenshot capture failed"));
			}
			return Result;
		});
}

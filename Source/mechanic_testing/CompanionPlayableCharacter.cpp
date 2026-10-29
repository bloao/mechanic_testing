#include "CompanionPlayableCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

ACompanionPlayableCharacter::ACompanionPlayableCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 500.0, 0.0);
	GetCharacterMovement()->JumpZVelocity = 700.0f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	// The companion controller creates AI only for the inactive character.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = AAIController::StaticClass();
	bCanDoubleJump = true;
	JumpMaxCount = 2;
	SmashKey = EKeys::E;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -96.0));
	GetMesh()->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));

	AgileMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")));
	StrongMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	AgileAnimationClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Animations/ABP_Agile.ABP_Agile_C")));
	StrongAnimationClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C")));
	MoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Move.IA_Move")));
	LookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Look.IA_Look")));
	MouseLookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook")));
	JumpAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump")));
	MouseLookMapping = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook")));
}

void ACompanionPlayableCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetWorld()->IsGameWorld())
	{
		ApplyAppearance();
	}
}

void ACompanionPlayableCharacter::ConfigureAgileAbilities(bool bIsAgile, int32 AgileJumpCount)
{
	Super::ConfigureAgileAbilities(bIsAgile, AgileJumpCount);
	if (IsActorInitialized())
	{
		ApplyAppearance();
	}
}

void ACompanionPlayableCharacter::ApplyAppearance()
{
	USkeletalMesh* RoleMesh = (bCanDoubleJump ? AgileMesh : StrongMesh).LoadSynchronous();
	UClass* RoleAnimation = (bCanDoubleJump ? AgileAnimationClass : StrongAnimationClass).LoadSynchronous();
	if (RoleMesh && RoleAnimation)
	{
		GetMesh()->SetSkeletalMesh(RoleMesh);
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(RoleAnimation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: companion mesh or animation is missing. Run git lfs pull and check Appearance settings."), *GetName());
	}
}

void ACompanionPlayableCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UInputAction* LoadedMove = MoveAction.LoadSynchronous();
	UInputAction* LoadedJump = JumpAction.LoadSynchronous();
	if (!EnhancedInput || !LoadedMove || !LoadedJump)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Enhanced Input or the Move/Jump actions are missing."), *GetName());
		return;
	}
	EnhancedInput->BindAction(LoadedMove, ETriggerEvent::Triggered, this, &ACompanionPlayableCharacter::Move);
	EnhancedInput->BindAction(LoadedJump, ETriggerEvent::Started, this, &ACompanionPlayableCharacter::Jump);
	EnhancedInput->BindAction(LoadedJump, ETriggerEvent::Completed, this, &ACompanionPlayableCharacter::StopJumping);
	EnhancedInput->BindAction(LoadedJump, ETriggerEvent::Canceled, this, &ACompanionPlayableCharacter::StopJumping);
	for (const TSoftObjectPtr<UInputAction>& Action : {LookAction, MouseLookAction})
	{
		if (UInputAction* LoadedLook = Action.LoadSynchronous())
		{
			EnhancedInput->BindAction(LoadedLook, ETriggerEvent::Triggered, this, &ACompanionPlayableCharacter::Look);
		}
	}
	// Use an assigned Enhanced Input action when present, otherwise a configurable prototype key.
	if (!SmashInputAction)
	{
		PlayerInputComponent->BindKey(SmashKey, IE_Pressed, this, &ACompanionPlayableCharacter::Smash);
	}
}

void ACompanionPlayableCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (UInputMappingContext* Mapping = MouseLookMapping.LoadSynchronous())
			{
				InputSubsystem->AddMappingContext(Mapping, 0);
			}
		}
	}
}

void ACompanionPlayableCharacter::UnPossessed()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (UInputMappingContext* Mapping = MouseLookMapping.Get())
			{
				InputSubsystem->RemoveMappingContext(Mapping);
			}
		}
	}
	Super::UnPossessed();
}

void ACompanionPlayableCharacter::Move(const FInputActionValue& Value)
{
	if (GetController())
	{
		const FVector2D Input = Value.Get<FVector2D>();
		const FRotator YawRotation(0.0, GetControlRotation().Yaw, 0.0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
	}
}

void ACompanionPlayableCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ACompanionPlayableCharacter::Smash()
{
	PerformSmash();
}

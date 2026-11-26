#include "CompanionPlayableCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogContainerPush, Log, All);

static TAutoConsoleVariable<int32> CVarContainerPushDebug(
	TEXT("companion.ContainerPush.Debug"), 0, TEXT("Log container grab targeting and blocked movement."));

namespace
{
	float GetAllowedMoveFraction(UPrimitiveComponent& Component, const UPrimitiveComponent& Partner,
		const FVector& Delta, FHitResult& BlockingHit)
	{
		FComponentQueryParams Query(SCENE_QUERY_STAT(CompanionContainerMove), Component.GetOwner());
		Query.AddIgnoredComponent(&Partner);
		TArray<FHitResult> Hits;
		const FVector Start = Component.GetComponentLocation();
		Component.GetWorld()->ComponentSweepMulti(Hits, &Component, Start, Start + Delta, Component.GetComponentQuat(), Query);

		const FVector Direction = Delta.GetSafeNormal();
		float Fraction = 1.0f;
		for (const FHitResult& Hit : Hits)
		{
			// Contact with the floor should still allow a horizontal slide. An
			// initial overlap blocks motion only when it would deepen the overlap.
			if (!Hit.bBlockingHit || (Hit.bStartPenetrating && FVector::DotProduct(Direction, Hit.Normal) >= -KINDA_SMALL_NUMBER))
			{
				continue;
			}
			if (Hit.Time < Fraction)
			{
				Fraction = Hit.Time;
				BlockingHit = Hit;
			}
		}
		return Fraction;
	}
}

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
	PlayerInputComponent->BindKey(PushContainerKey, IE_Pressed, this, &ACompanionPlayableCharacter::TryGrabContainer);
	PlayerInputComponent->BindKey(PushContainerKey, IE_Released, this, &ACompanionPlayableCharacter::ReleaseContainer);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UInputAction* LoadedMove = MoveAction.LoadSynchronous();
	UInputAction* LoadedJump = JumpAction.LoadSynchronous();
	if (!EnhancedInput || !LoadedMove || !LoadedJump)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Enhanced Input or the Move/Jump actions are missing."), *GetName());
		return;
	}
	EnhancedInput->BindAction(LoadedMove, ETriggerEvent::Triggered, this, &ACompanionPlayableCharacter::Move);
	EnhancedInput->BindAction(LoadedMove, ETriggerEvent::Completed, this, &ACompanionPlayableCharacter::ClearContainerMoveInput);
	EnhancedInput->BindAction(LoadedMove, ETriggerEvent::Canceled, this, &ACompanionPlayableCharacter::ClearContainerMoveInput);
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
	ReleaseContainer();
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
	const FVector2D Input = Value.Get<FVector2D>();
	if (IsHoldingContainer())
	{
		ContainerForwardInput = FMath::Clamp(Input.Y, -1.0, 1.0);
		return;
	}
	if (GetController())
	{
		const FRotator YawRotation(0.0, GetControlRotation().Yaw, 0.0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
	}
}

void ACompanionPlayableCharacter::ClearContainerMoveInput()
{
	ContainerForwardInput = 0.0;
}

void ACompanionPlayableCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsHoldingContainer() && bCanSmash && HasTemporaryGrowth() && GetCharacterMovement()->IsMovingOnGround())
	{
		MoveHeldContainer(DeltaSeconds);
	}
	else
	{
		ReleaseContainer();
	}
}

void ACompanionPlayableCharacter::Jump()
{
	ReleaseContainer();
	Super::Jump();
}

void ACompanionPlayableCharacter::ReleaseContainer()
{
	HeldContainer.Reset();
	ClearContainerMoveInput();
	ContainerMoveVelocity = FVector2D::ZeroVector;
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

void ACompanionPlayableCharacter::TryGrabContainer()
{
	if (IsHoldingContainer())
	{
		return;
	}
	const bool bDebug = CVarContainerPushDebug.GetValueOnGameThread() != 0;
	const bool bHasGrowth = HasTemporaryGrowth();
	if (!bCanSmash || !bHasGrowth || !GetCharacterMovement()->IsMovingOnGround())
	{
		if (bDebug)
		{
			UE_LOG(LogContainerPush, Display, TEXT("Rejected %s: Strong=%d, growth active=%d, grounded=%d."),
				*GetName(), bCanSmash, bHasGrowth, GetCharacterMovement()->IsMovingOnGround());
		}
		return;
	}

	constexpr float PushReach = 150.0f;
	constexpr float PushAimRadius = 30.0f;
	static const FName LargeContainerMeshName(TEXT("SM_ContainerClosed_Large"));

	// Looking around does not rotate the pawn until it moves. Use the same
	// camera-relative forward direction as the movement input.
	const FVector AimDirection = FRotator(0.0, GetControlRotation().Yaw, 0.0).Vector();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Start = GetActorLocation() - FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() * 0.5f);
	const FVector End = Start + AimDirection * (Capsule->GetScaledCapsuleRadius() + PushReach);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(CompanionContainerPush), false, this);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(PushAimRadius), Query))
	{
		if (bDebug)
		{
			UE_LOG(LogContainerPush, Display, TEXT("Aim missed. Start=%s End=%s"),
				*Start.ToCompactString(), *End.ToCompactString());
		}
		return;
	}

	UStaticMeshComponent* Container = Cast<UStaticMeshComponent>(Hit.GetComponent());
	const UStaticMesh* HitMesh = Container ? Container->GetStaticMesh() : nullptr;
	if (!HitMesh || HitMesh->GetFName() != LargeContainerMeshName)
	{
		if (bDebug)
		{
			UE_LOG(LogContainerPush, Display, TEXT("Aim hit a different target. Actor=%s Component=%s Mesh=%s"),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *GetNameSafe(HitMesh));
		}
		return;
	}

	if (Container->IsSimulatingPhysics() || !Container->IsQueryCollisionEnabled())
	{
		if (bDebug)
		{
			UE_LOG(LogContainerPush, Display, TEXT("Grab needs a container with query collision and physics simulation off."));
		}
		return;
	}

	const FVector PushDirection = -Hit.ImpactNormal.GetSafeNormal2D();
	Container->SetMobility(EComponentMobility::Movable);
	ContainerForward = PushDirection.IsNearlyZero() ? AimDirection : PushDirection;
	GetCharacterMovement()->StopMovementImmediately();
	ConsumeMovementInputVector();
	SetActorRotation(ContainerForward.Rotation());
	HeldContainer = Container;
	if (bDebug)
	{
		UE_LOG(LogContainerPush, Display, TEXT("Grabbed %s. Forward=%s"), *GetNameSafe(Hit.GetActor()), *ContainerForward.ToCompactString());
	}
}

void ACompanionPlayableCharacter::MoveHeldContainer(float DeltaSeconds)
{
	ContainerMoveVelocity = FVector2D::ZeroVector;
	if (DeltaSeconds <= SMALL_NUMBER || FMath::IsNearlyZero(ContainerForwardInput))
	{
		return;
	}

	UStaticMeshComponent* Container = HeldContainer.Get();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	constexpr float HoldMoveSpeed = 100.0f;
	const double ForwardSpeed = ContainerForwardInput * HoldMoveSpeed;
	const FVector Requested = ContainerForward * ForwardSpeed * DeltaSeconds;
	FHitResult ContainerHit;
	FHitResult CharacterHit;
	const float ContainerFraction = GetAllowedMoveFraction(*Container, *Capsule, Requested, ContainerHit);
	const float CharacterFraction = GetAllowedMoveFraction(*Capsule, *Container, Requested, CharacterHit);
	const float Fraction = FMath::Min(ContainerFraction, CharacterFraction);
	const FVector Delta = Requested * Fraction;

	// Both sweeps have already been checked. Move both by the same delta so
	// neither slides ahead when its partner reaches an obstacle.
	if (!Delta.IsNearlyZero())
	{
		Container->AddWorldOffset(Delta);
		AddActorWorldOffset(Delta);
		GetCharacterMovement()->bForceNextFloorCheck = true;
		ContainerMoveVelocity.Y = ForwardSpeed * Fraction;
	}
	if (Fraction < 1.0f && CVarContainerPushDebug.GetValueOnGameThread() != 0)
	{
		const FHitResult& Block = ContainerFraction <= CharacterFraction ? ContainerHit : CharacterHit;
		UE_LOG(LogContainerPush, Display, TEXT("%s movement blocked by %s (%s). Initial overlap=%d"),
			ContainerFraction <= CharacterFraction ? TEXT("Container") : TEXT("Strong"),
			*GetNameSafe(Block.GetActor()), *GetNameSafe(Block.GetComponent()), Block.bStartPenetrating);
	}
}

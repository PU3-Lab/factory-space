// Fill out your copyright notice in the Description page of Project Settings.

#include "Dummy_MachineBase.h"

#include "Components/TextRenderComponent.h"

namespace
{
FString FormatItemMap(const TMap<FName, int32>& Items)
{
	if (Items.Num() == 0)
	{
		return TEXT("None");
	}

	FString Result;
	for (const TPair<FName, int32>& Item : Items)
	{
		if (Item.Key.IsNone() || Item.Value <= 0)
		{
			continue;
		}

		if (!Result.IsEmpty())
		{
			Result += TEXT("\n");
		}
		Result += FString::Printf(TEXT("%s x%d"), *Item.Key.ToString(), Item.Value);
	}

	return Result.IsEmpty() ? FString(TEXT("None")) : Result;
}
}

ADummyMachineBase::ADummyMachineBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	DebugBufferText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugBufferText"));
	DebugBufferText->SetupAttachment(RootComponent);
	DebugBufferText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugBufferText->SetHorizontalAlignment(EHTA_Center);
	DebugBufferText->SetVerticalAlignment(EVRTA_TextCenter);
	DebugBufferText->SetWorldSize(DebugTextWorldSize);
	DebugBufferText->SetRelativeLocation(DebugTextOffset);
	DebugBufferText->SetRelativeRotation(FRotator(60.0f, 0.0f, 0.0f));
}

void ADummyMachineBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateDebugBufferText();
}

void ADummyMachineBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateDebugBufferText();
}

bool ADummyMachineBase::PeekFirstOutputItem(FName& OutItemID) const
{
	OutItemID = NAME_None;

	for (const TPair<FName, int32>& Output : OutputBuffer)
	{
		if (!Output.Key.IsNone() && Output.Value > 0)
		{
			OutItemID = Output.Key;
			return true;
		}
	}

	return false;
}

bool ADummyMachineBase::TryTakeFirstOutputItem(FName& OutItemID)
{
	if (!PeekFirstOutputItem(OutItemID))
	{
		return false;
	}

	if (!TakeOutputItem(OutItemID, 1))
	{
		OutItemID = NAME_None;
		return false;
	}

	return true;
}

bool ADummyMachineBase::CanReceiveConveyorItem(FName ItemID, int32 Count) const
{
	return CanAddInputItem(ItemID, Count);
}

bool ADummyMachineBase::ReceiveConveyorItem(FName ItemID, int32 Count)
{
	const bool bAdded = AddItem(ItemID, Count);
	UpdateDebugBufferText();
	return bAdded;
}

void ADummyMachineBase::UpdateDebugBufferText()
{
	if (!DebugBufferText)
	{
		return;
	}

	DebugBufferText->SetVisibility(bShowDebugBufferText);
	DebugBufferText->SetWorldSize(DebugTextWorldSize);
	DebugBufferText->SetRelativeLocation(DebugTextOffset);
	if (!bShowDebugBufferText)
	{
		return;
	}

	const FString DebugText = FString::Printf(
		TEXT("Input\n%s\n\nOutput\n%s"),
		*FormatItemMap(InputInventory),
		*FormatItemMap(OutputBuffer));
	DebugBufferText->SetText(FText::FromString(DebugText));
}

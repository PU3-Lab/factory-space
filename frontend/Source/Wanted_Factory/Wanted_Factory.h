// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogWanted_Factory, Log, All);

// =====================
// LC (이찬)
// =====================
#define LOG_LC(Format, ...) \
UE_LOG(LogWanted_Factory, Log, TEXT("[LC][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_LC_W(Format, ...) \
UE_LOG(LogWanted_Factory, Warning, TEXT("[LC][W][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_LC_E(Format, ...) \
UE_LOG(LogWanted_Factory, Error, TEXT("[LC][E][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))


// =====================
// SSR (서승률)
// =====================
#define LOG_SSR(Format, ...) \
UE_LOG(LogWanted_Factory, Log, TEXT("[SSR][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_SSR_W(Format, ...) \
UE_LOG(LogWanted_Factory, Warning, TEXT("[SSR][W][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_SSR_E(Format, ...) \
UE_LOG(LogWanted_Factory, Error, TEXT("[SSR][E][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))


// =====================
// OJJ (오재준)
// =====================
#define LOG_OJJ(Format, ...) \
UE_LOG(LogWanted_Factory, Log, TEXT("[OJJ][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_OJJ_W(Format, ...) \
UE_LOG(LogWanted_Factory, Warning, TEXT("[OJJ][W][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_OJJ_E(Format, ...) \
UE_LOG(LogWanted_Factory, Error, TEXT("[OJJ][E][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

// =====================
// LDJ (이동진)
// =====================
#define LOG_LDJ(Format, ...) \
UE_LOG(LogWanted_Factory, Log, TEXT("[LDJ][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_LDJ_W(Format, ...) \
UE_LOG(LogWanted_Factory, Warning, TEXT("[LDJ][W][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))

#define LOG_LDJ_E(Format, ...) \
UE_LOG(LogWanted_Factory, Error, TEXT("[LDJ][E][%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(Format, ##__VA_ARGS__))
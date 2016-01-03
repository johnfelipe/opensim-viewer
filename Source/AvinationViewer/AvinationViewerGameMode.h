// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "GameFramework/GameMode.h"
#include "MeshActor.h"
#include "AvinationViewerGameMode.generated.h"
class ObjectCreator;

/**
 * 
 */
UCLASS()
class AVINATIONVIEWER_API AAvinationViewerGameMode : public AGameMode
{
	GENERATED_BODY()
	
	
	virtual void HandleMatchHasStarted() override;
    virtual void Tick(float deltaSeconds) override;
    
private:
    TMap<FGuid, AMeshActor *> actors;
    TArray<rapidxml::xml_node<> *> queue;
    
    void CreateNewActor(FGuid id, TArray<uint8_t> data);
    AMeshActor *CreateNewActor(rapidxml::xml_node<> *data, ObjectReadyDelegate d, AMeshActor *act = 0);
    AMeshActor *CreateNewActor(rapidxml::xml_node<> *data);
    void HandleObjectReady(AMeshActor *act);
    ObjectCreator *creator = 0;
    
    friend class ObjectCreator;
};

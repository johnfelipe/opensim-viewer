// Fill out your copyright notice in the Description page of Project Settings.

#include "AvinationViewer.h"
#include "SceneObjectPart.h"
#include "SceneObjectGroup.h"
#include "AssetCache.h"
#include "AssetDecode.h"
#include "LLSDMeshDecode.h"
#include <cstdlib>
#include "Base64.h"
#include "PrimMesher.h"
#include "J2KDecode.h"
#include "openjpeg.h"

SceneObjectPart::SceneObjectPart()
{
    isPrim = isMesh = isSculpt = false;
    haveAllAssets = false;
    numFaces = 0;
    primData = 0;
    sculptData = 0;
}

SceneObjectPart::~SceneObjectPart()
{
    if (primData)
        delete primData;
    if (sculptData)
        delete sculptData;
}

float SceneObjectPart::ReadFloatValue(rapidxml::xml_node<> *parent, const char *name, float def)
{
    rapidxml::xml_node<> *node = parent->first_node(name);
    if (!node)
        return def;
    if (!node->value())
        return def;
    return atof(node->value());
}

int SceneObjectPart::ReadIntValue(rapidxml::xml_node<> *parent, const char *name, int def)
{
    rapidxml::xml_node<> *node = parent->first_node(name);
    if (!node)
        return def;
    if (!node->value())
        return def;
    return atoi(node->value());
}

FString SceneObjectPart::ReadStringValue(rapidxml::xml_node<> *parent, const char *name, FString def)
{
    rapidxml::xml_node<> *node = parent->first_node(name);
    if (!node)
        return def;
    if (!node->value())
        return def;
    return FString(node->value());
}

bool SceneObjectPart::Load(rapidxml::xml_node<> *xml)
{
    rapidxml::xml_node<> *uuidNode = xml->first_node("UUID");
    rapidxml::xml_node<> *innerUuidNode = uuidNode->first_node();
    
    FGuid::Parse(FString(innerUuidNode->value()), uuid);
    
    rapidxml::xml_node<> *shapeNode = xml->first_node("Shape");
    if (!shapeNode)
        return false;
    rapidxml::xml_node<> *sculptTypeNode = shapeNode->first_node("SculptType");
    rapidxml::xml_node<> *sculptTextureNode = shapeNode->first_node("SculptTexture");
    
    if (!sculptTypeNode || !sculptTextureNode)
        return false;
    
    innerUuidNode = sculptTextureNode->first_node();
    if (!innerUuidNode)
        return false;
    
    rapidxml::xml_node<> *textureEntryNode = shapeNode->first_node("TextureEntry");
    if (!textureEntryNode)
        return false;
    FString txe(textureEntryNode->value());
    
    FBase64::Decode(txe, textureEntry);
    
    TextureEntry::Parse(textureEntry, defaultTexture, textures);
    
    pathBegin = ReadFloatValue(shapeNode, "PathBegin", 0);
    pathEnd = ReadFloatValue(shapeNode, "PathEnd", 0);
    pathRadiusOffset = ReadFloatValue(shapeNode, "PathRadiusOffset", 0);
    pathRevolutions = ReadFloatValue(shapeNode, "PathRevolutions", 0);
    pathScaleX = ReadFloatValue(shapeNode, "PathScaleX", 100);
    pathScaleY = ReadFloatValue(shapeNode, "PathScaleY", 100);
    pathShearX = ReadFloatValue(shapeNode, "PathShearX", 0);
    pathShearY = ReadFloatValue(shapeNode, "PathShearY", 0);
    pathSkew = ReadFloatValue(shapeNode, "PathSkew", 0);
    pathTaperX = ReadFloatValue(shapeNode, "PathTaperX", 0);
    pathTaperY = ReadFloatValue(shapeNode, "PathTaperY", 0);
    pathTwist = ReadFloatValue(shapeNode, "PathTiwst", 0);
    pathTwistBegin = ReadFloatValue(shapeNode, "PathTwistBegin", 0);
    profileBegin = ReadFloatValue(shapeNode, "ProfileBegin", 0);
    profileEnd = ReadFloatValue(shapeNode, "ProfileEnd", 0);
    profileHollow = ReadFloatValue(shapeNode, "ProfileHollow", 0);
    pathCurve = ReadIntValue(shapeNode, "PathCurve", 16);
    
    FString txt = ReadStringValue(shapeNode, "ProfileShape", "Square");
    if (txt == "Circle")
        profileShape = 0;
    else if (txt == "Square")
        profileShape = 1;
    else if (txt == "IsometricTriangle" || txt == "IsoTriangle")
        profileShape = 2;
    else if (txt == "EquilateralTriangle" || txt == "EqualTriangle")
        profileShape = 3;
    else if (txt == "RightTriangle")
        profileShape = 4;
    else if (txt == "HalfCircle")
        profileShape = 5;
    else
        profileShape = 0;
    
    txt = ReadStringValue(shapeNode, "HollowShape", "Square");
    if (txt == "Same")
        hollowShape = 0;
    else if (txt == "Circle")
        hollowShape = 0x10;
    else if (txt == "Square")
        hollowShape = 0x20;
    else if (txt == "Triangle")
        hollowShape = 0x30;
    else
        hollowShape = 0;
    
    rapidxml::xml_node<> *scaleNode = xml->first_node("Scale");
    if (!scaleNode)
        return false;
    
    rapidxml::xml_node<> *x, *y, *z, *w;
    x = scaleNode->first_node("X");
    y = scaleNode->first_node("Y");
    z = scaleNode->first_node("Z");
    
    if (!x || !y || !z)
        return false;
    
    scale.X = atof(x->value());
    scale.Y = atof(y->value());
    scale.Z = atof(z->value());
    
    if (scale.X == 0)
        scale.X = 0.001;
    if (scale.Y == 0)
        scale.Y = 0.001;
    if (scale.Z == 0)
        scale.Z = 0.001;
    
    if (fabs(scale.X) < 1 && fabs(scale.Y) < 1 && fabs(scale.Z) < 1)
        maxLod = 1;
    if (fabs(scale.X) < 0.1 && fabs(scale.Y) < 0.1 && fabs(scale.Z) < 0.1)
        maxLod = 2;
    
    rapidxml::xml_node<> *rotationNode = xml->first_node("RotationOffset");
    
    if (!rotationNode)
        return false;
    
    x = rotationNode->first_node("X");
    y = rotationNode->first_node("Y");
    z = rotationNode->first_node("Z");
    w = rotationNode->first_node("W");
    
    if (!x || !y || !z || !w)
        return false;
    
    rotation.X = atof(x->value());
    rotation.Y = -atof(y->value());
    rotation.Z = atof(z->value());
    rotation.W = -atof(w->value());
    
    rotation.Normalize();
    
    rapidxml::xml_node<> *positionNode = xml->first_node("OffsetPosition");
    if (!positionNode)
        return false;
    
    x = positionNode->first_node("X");
    y = positionNode->first_node("Y");
    z = positionNode->first_node("Z");
    
    if (!x || !y || !z)
        return false;
    
    position.X = atof(x->value());
    position.Y = -atof(y->value());
    position.Z = atof(z->value());
    
    rapidxml::xml_node<> *groupPositionNode = xml->first_node("GroupPosition");
    if (!groupPositionNode)
        return false;
    
    x = groupPositionNode->first_node("X");
    y = groupPositionNode->first_node("Y");
    z = groupPositionNode->first_node("Z");
    
    if (!x || !y || !z)
        return false;
    
    groupPosition.X = atof(x->value());
    groupPosition.Y = -atof(y->value());
    groupPosition.Z = atof(z->value());
    
    sculptType = atoi(sculptTypeNode->value());
                      
    if ((sculptType & 0x3f) == 5) // Mesh
    {
        isMesh = true;
        
        meshAssetId = FString(innerUuidNode->value());
    }
    else if ((sculptType & 0x3f) == 0) // Prim
    {
        isPrim = true;
    }
    else
    {
        isSculpt = true;
        
        meshAssetId = FString(innerUuidNode->value());
    }
    
    return true;
}

void SceneObjectPart::AssetReceived(FGuid id, TArray<uint8_t> data)
{
    haveAllAssets = true;
    
    group->fetchLock.Lock();
    --group->activeFetches;
    group->fetchLock.Unlock();

    if ((sculptType & 0x3f) != 5) // Sculpt
    {
        MeshSculpt(data);
    }
    else
    {
        meshAssetData = data;
    }
    
    group->CheckAssetsDone();
}

void SceneObjectPart::FetchAssets()
{
    FGuid id;
    
    if ((sculptType & 0x3f) == 0) // Prim
    {
        // Prims need no shape assets
        group->CheckAssetsDone();
    }
    else if ((sculptType & 0x3f) == 5) // Mesh
    {
        FGuid::Parse(meshAssetId, id);
        OnAssetFetched d;
        d.BindRaw(this, &SceneObjectPart::AssetReceived);
        group->fetchLock.Lock();
        ++group->activeFetches;
        //UE_LOG(LogTemp, Warning, TEXT("Fetches now %d"), group->activeFetches);
        group->fetchLock.Unlock();
        AssetCache::Get().Fetch(id, d);
    }
    else // Sculpt
    {
        // The mesh asset is here the sculpt texture
        FGuid::Parse(meshAssetId, id);
        OnAssetFetched d;
        d.BindRaw(this, &SceneObjectPart::AssetReceived);
        group->fetchLock.Lock();
        ++group->activeFetches;
        //UE_LOG(LogTemp, Warning, TEXT("Fetches now %d"), group->activeFetches);
        group->fetchLock.Unlock();
        AssetCache::Get().Fetch(id, d);
    }
}

LLSDItem *SceneObjectPart::GetMeshData(int lod)
{
    if ((sculptType & 0x3f) == 5) // Mesh
    {
        AssetDecode *dec;
        try
        {
            dec = new AssetDecode(meshAssetData);
        }
        catch (asset_decode_exception ex)
        {
            return 0;
        }
        
        TArray<uint8_t> data = dec->AsBase64DecodeArray();
        
        if (lod < maxLod)
            lod = maxLod;
        
        FString lodLevel = TEXT("high_lod");
        if (lod == 1)
            lodLevel = TEXT("medium_lod");
        else if (lod == 2)
            lodLevel = TEXT("low_lod");
        
        LLSDItem *lodData = LLSDMeshDecode::Decode(data.GetData(), TEXT("high_lod"));
        
        delete dec;
        
        numFaces = lodData->arrayData.Num();
        
        return lodData;
    }
    
    return 0;
}

inline static bool SortByFace(const ViewerFace& f1, const ViewerFace& f2)
{
    return f1.primFaceNumber < f2.primFaceNumber;
}

bool SceneObjectPart::MeshPrim()
{
    int lodWanted = 0;
    if (lodWanted < maxLod)
        lodWanted = maxLod;
    
    if (meshed)
        return true;
    
    try
    {
        GeneratePrimMesh(lodWanted);
    }
//    catch (std::exception& ex)
    catch (...)
    {
        return false;
    }
    
    primData->viewerFaces.Sort(SortByFace);
    
    TArray<ViewerFace> faces;
    
    int face = -1;
    int prevFace = -1;
    for (int viewerFace = 0 ; viewerFace < primData->viewerFaces.Num() ; viewerFace++ )
    {
        if (primData->viewerFaces[viewerFace].v1 == primData->viewerFaces[viewerFace].v2 ||
            primData->viewerFaces[viewerFace].v1 == primData->viewerFaces[viewerFace].v3 ||
            primData->viewerFaces[viewerFace].v2 == primData->viewerFaces[viewerFace].v3)
        {
            // This is not a rational triangle
            continue;
        }
        
        if (primData->viewerFaces[viewerFace].primFaceNumber != prevFace)
        {
            face++;
            prevFace = primData->viewerFaces[viewerFace].primFaceNumber;
        }
        primData->viewerFaces[viewerFace].primFaceNumber = face;
        faces.Add(primData->viewerFaces[viewerFace]);
    }
    primData->viewerFaces = faces;
    primData->numPrimFaces = face + 1;
    
    numFaces = face + 1;
    meshed = true;
    return true;
}

void SceneObjectPart::GeneratePrimMesh(int lod)
{
    pathShearX = pathShearX < 128 ? (float)pathShearX * 0.01f : (float)(pathShearX - 256) * 0.01f;
    pathShearY = pathShearY < 128 ? (float)pathShearY * 0.01f : (float)(pathShearY - 256) * 0.01f;
    pathBegin = (float)pathBegin * 2.0e-5f;
    pathEnd = 1.0f - pathEnd * 2.0e-5f;
    pathScaleX = (float)(pathScaleX - 100) * 0.01f;
    pathScaleY = (float)(pathScaleY - 100) * 0.01f;
    
    profileBegin = (float)profileBegin * 2.0e-5f;
    profileEnd = 1.0f - (float)profileEnd * 2.0e-5f;
    profileHollow = (float)profileHollow * 2.0e-5f;
    
    if (profileHollow > 0.95f)
        profileHollow = 0.95f;
    
    int sides = 4;
    int hollowsides = 4;
    
    bool isSphere = false;
    
    if ((profileShape & 0x07) == 0)
    {
        switch (lod)
        {
            case 2:
                sides = 6;
                break;
            case 1:
                sides = 12;
                break;
            default:
                sides = 24;
                break;
        }
    }
    else if ((profileShape & 0x07) == 3)
        sides = 3;
    else if ((profileShape & 0x07) == 5)
    {
        // half circle, prim is a sphere
        isSphere = true;
        switch (lod)
        {
            case 2:
                sides = 6;
                break;
            case 3:
                sides = 12;
                break;
            default:
                sides = 24;
                break;
        }
        profileBegin = 0.5f * profileBegin + 0.5f;
        profileEnd = 0.5f * profileEnd + 0.5f;
    }
    if (hollowShape == 0)
        hollowsides = sides;
    else if (hollowShape == 0x10)
    {
        switch (lod)
        {
            case 2:
                hollowsides = 6;
                break;
            case 1:
                hollowsides = 12;
                break;
            default:
                hollowsides = 24;
                break;
        }
    }
    else if (hollowShape == 0x30)
        hollowsides = 3;
    
    PrimMesh *newPrim = new PrimMesh(sides, profileBegin, profileEnd, hollowShape, hollowsides);
    newPrim->viewerMode = true;
    newPrim->sphereMode = isSphere;
    newPrim->holeSizeX = pathScaleX;
    newPrim->holeSizeY = pathScaleY;
    newPrim->pathCutBegin = pathBegin;
    newPrim->pathCutEnd = pathEnd;
    newPrim->topShearX = pathShearX;
    newPrim->topShearY = pathShearY;
    newPrim->radius = pathRadiusOffset;
    newPrim->revolutions = pathRevolutions;
    newPrim->skew = pathSkew;
    newPrim->hollow = profileHollow;
    
    
    switch (lod)
    {
        case 2:
            newPrim->stepsPerRevolution = 6;
            break;
        case 1:
            newPrim->stepsPerRevolution = 12;
            break;
        default:
            newPrim->stepsPerRevolution = 24;
            break;
    }
    
    if ((pathCurve == 0x10) || (pathCurve == 0x80))
    {
        newPrim->twistBegin = pathTwistBegin * 18 / 10;
        newPrim->twistEnd =pathTwist * 18 / 10;
        newPrim->taperX = pathScaleX;
        newPrim->taperY = pathScaleY;
        if (profileBegin < 0.0f || profileBegin >= profileEnd || profileEnd > 1.0f)
        {
            if (profileBegin < 0.0f) profileBegin = 0.0f;
            if (profileEnd > 1.0f) profileEnd = 1.0f;
        }
        newPrim->ExtrudeLinear();
    }
    else
    {
        newPrim->holeSizeX = (200 - pathScaleX) * 0.01f;
        newPrim->holeSizeY = (200 - pathScaleY) * 0.01f;
        newPrim->radius = 0.01f * pathRadiusOffset;
        newPrim->revolutions = 1.0f + 0.015f * pathRevolutions;
        newPrim->skew = 0.01f * pathSkew;
        newPrim->twistBegin = pathTwistBegin * 36 / 10;
        newPrim->twistEnd = pathTwist * 36 / 10;
        newPrim->taperX = pathTaperX * 0.01f;
        newPrim->taperY = pathTaperY * 0.01f;
        
        if (profileBegin < 0.0f || profileBegin >= profileEnd || profileEnd > 1.0f)
        {
            if (profileBegin < 0.0f) profileBegin = 0.0f;
            if (profileEnd > 1.0f) profileEnd = 1.0f;
        }
        newPrim->ExtrudeCircular();
    }
    
    primData = newPrim;
    
    /*
    for (auto it = primData->viewerFaces.CreateConstIterator() ; it ; ++it)
    {
        UE_LOG(LogTemp, Warning, TEXT("Face %d (%f, %f, %f) - (%f, %f, %f) - (%f, %f, %f)"), (*it).primFaceNumber,
               (*it).v1.X,
               (*it).v1.Y,
               (*it).v1.Z,
               (*it).v2.X,
               (*it).v2.Y,
               (*it).v2.Z,
               (*it).v3.X,
               (*it).v3.Y,
               (*it).v3.Z
               );
    }
    */
}

bool SceneObjectPart::MeshSculpt(TArray<uint8_t> data)
{
    if (meshed)
        return true;
    
    try
    {
        GenerateSculptMesh(data);
    }
/*
    catch (asset_decode_exception& ex)
    {
        return false;
    }
    catch (std::exception& ex)
    {
    return false;
    }
*/
    catch (...)
    {
        return false;
    }

    numFaces = 1;
    
    meshed = true;
    return true;
}

void SceneObjectPart::GenerateSculptMesh(TArray<uint8_t> indata)
{
    bool mirror = (sculptType & 0x80) ? true : false;
    bool invert = (sculptType & 0x40) ? true : false;
    float pixScale = 1.0f / 256.0f;
    
    AssetDecode dec(indata);
    TArray<uint8_t> data = dec.AsBase64DecodeArray();
    J2KDecode *idec = new J2KDecode();
    idec->Decode(dec.AsBase64DecodeArray());
    opj_image_t *tex = idec->image;
    
    if (!tex)
        throw std::exception();
    
    if (tex->numcomps < 3)
        throw std::exception();
    
    int w = tex->comps[0].w;
    int h = tex->comps[0].h;

    TArray<TArray<Coord>> rows;
    OPJ_INT32 *r = tex->comps[0].data, *g = tex->comps[1].data, *b = tex->comps[2].data;
    for (int i = 0 ; i < h ; i++)
        rows.AddDefaulted();
    
    for (int i = 0 ; i < h ; i++)
    {
        for (int j = 0 ; j < w ; j++)
        {
            Coord c = Coord((float)(*r++ & 0xff) * pixScale - 0.5f,
                            (float)(*g++ & 0xff) * pixScale - 0.5f,
                            (float)(*b++ & 0xff) * pixScale - 0.5f);
            if (mirror)
                c.X = -c.X;
            rows[h - 1 - i].Add(c);
        }
    }
    
    sculptData = new SculptMesh(rows, (SculptType)sculptType, true, mirror, invert);
    
    delete idec;
}

void SceneObjectPart::DeleteMeshData()
{
    if (primData)
        delete primData;
    if (sculptData)
        delete sculptData;
    primData = 0;
    sculptData = 0;
    meshAssetData.Empty();
}

void SceneObjectPart::GatherTextures()
{
    for (int i = 0 ; i < numFaces ; ++i)
        group->AddTexture(textures[i].textureId);
}
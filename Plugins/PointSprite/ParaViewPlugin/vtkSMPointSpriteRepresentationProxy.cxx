/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkSMPointSpriteRepresentationProxy.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// .NAME vtkSMPointSpriteRepresentationProxy
// .SECTION Thanks
// <verbatim>
//
//  This file is part of the PointSprites plugin developed and contributed by
//
//  Copyright (c) CSCS - Swiss National Supercomputing Centre
//                EDF - Electricite de France
//
//  John Biddiscombe, Ugo Varetto (CSCS)
//  Stephane Ploix (EDF)
//
// </verbatim>

#include "vtkSMPointSpriteRepresentationProxy.h"

#include "vtkAbstractMapper.h"
#include "vtkObjectFactory.h"
#include "vtkGlyphSource2D.h"
#include "vtkProperty.h"

#include "vtkProcessModule.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSmartPointer.h"
#include "vtkType.h"
#include "vtkPVDataInformation.h"
#include "vtkSMProxyIterator.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMDoubleVectorProperty.h"

using vtkstd::string;

vtkStandardNewMacro(vtkSMPointSpriteRepresentationProxy)
//----------------------------------------------------------------------------
vtkSMPointSpriteRepresentationProxy::vtkSMPointSpriteRepresentationProxy()
{
  this->DefaultsInitialized = false;
}

//----------------------------------------------------------------------------
vtkSMPointSpriteRepresentationProxy::~vtkSMPointSpriteRepresentationProxy()
{
}

//----------------------------------------------------------------------------
void vtkSMPointSpriteRepresentationProxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }
  this->Superclass::CreateVTKObjects();
  if (!this->ObjectsCreated)
    {
    return;
    }
}

//----------------------------------------------------------------------------
void vtkSMPointSpriteRepresentationProxy::RepresentationUpdated()
{
  this->Superclass::RepresentationUpdated();
  if (!this->DefaultsInitialized)
    {
    this->InitializeDefaultValues();
    this->DefaultsInitialized = true;
    }
}

//----------------------------------------------------------------------------
void vtkSMPointSpriteRepresentationProxy::InitializeDefaultValues()
{
  // Initialize the default radius if needed.
  bool radiusInitialized = vtkSMIntVectorProperty::SafeDownCast(this->GetProperty(
      "RadiusInitialized"))->GetElement(0) != 0;

  if (!radiusInitialized)
    {
    double radius = this->ComputeInitialRadius(
        this->GetRepresentedDataInformation());
    radius = 0.1831;
    vtkSMDoubleVectorProperty::SafeDownCast(this->GetProperty(
          "ConstantRadius"))->SetElements1(radius);
    vtkSMDoubleVectorProperty::SafeDownCast(this->GetProperty(
          "RadiusRange"))->SetElements2(0.0, radius);
    vtkSMIntVectorProperty::SafeDownCast(this->GetProperty(
          "RadiusInitialized"))->SetElements1(1);
    }

  // Initialize the Transfer functions if needed
  int nop = vtkSMVectorProperty::SafeDownCast(this->GetProperty("OpacityTableValues"))->GetNumberOfElements();
  if (nop == 0)
    {
    InitializeTableValues(this->GetProperty("OpacityTableValues"));
    }

  int nrad = vtkSMVectorProperty::SafeDownCast(this->GetProperty("RadiusTableValues"))->GetNumberOfElements();
  if (nrad == 0)
    {
    InitializeTableValues(this->GetProperty("RadiusTableValues"));
    }

  InitializeSpriteTextures();

  this->UpdateVTKObjects();
}

double vtkSMPointSpriteRepresentationProxy::ComputeInitialRadius(vtkPVDataInformation* info)
{
  vtkIdType npts = info->GetNumberOfPoints();
  if (npts == 0)
    npts = 1;
  double bounds[6];
  info->GetBounds(bounds);

  double diag = sqrt(((bounds[1] - bounds[0]) * (bounds[1] - bounds[0])
      + (bounds[3] - bounds[2]) * (bounds[3] - bounds[2]) + (bounds[5]
      - bounds[4]) * (bounds[5] - bounds[4])) / 3.0);

  double nn = pow(static_cast<double>(npts), 1.0 / 3.0) - 1.0;
  if (nn < 1.0)
    nn = 1.0;

  return diag / nn / 2.0;
}

void vtkSMPointSpriteRepresentationProxy::InitializeTableValues(vtkSMProperty* prop)
{
  vtkSMDoubleVectorProperty* tableprop = vtkSMDoubleVectorProperty::SafeDownCast(prop);
  tableprop->SetNumberOfElements(256);
  double values[256];
  for (int i = 0; i < 256; i++)
    {
    values[i] = ((double) i) / 255.0;
    }
  tableprop->SetElements(values);
}

void vtkSMPointSpriteRepresentationProxy::InitializeSpriteTextures()
{
  vtkSMProxyIterator* proxyIter;
  string texName;
  bool created;
  vtkSMProxy* texture;
  int extent[6] = {0, 65, 0, 65, 0, 0};
  vtkSMIntVectorProperty* extentprop;
  vtkSMIntVectorProperty* maxprop;
  vtkSMDoubleVectorProperty* devprop;
  vtkSMIntVectorProperty* alphamethodprop;
  vtkSMIntVectorProperty* alphathresholdprop;

  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();

  texName = "Sphere";
  created = false;
  proxyIter = vtkSMProxyIterator::New();
  proxyIter->SetModeToOneGroup();
  for (proxyIter->Begin("textures"); !proxyIter->IsAtEnd(); proxyIter->Next())
    {
    string name = proxyIter->GetKey();
    if (name == texName)
      {
      created = true;
      break;
      }
    }
  proxyIter->Delete();

  if (!created)
    {
    // create the texture proxy
    texture = pxm->NewProxy("textures", "SpriteTexture");
    texture->SetConnectionID(this->GetConnectionID());
    texture->SetServers(vtkProcessModule::CLIENT
        | vtkProcessModule::RENDER_SERVER);
    pxm->RegisterProxy("textures", texName.c_str(), texture);
    texture->Delete();

    // set the texture parameters
    extentprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("WholeExtent"));
    extentprop->SetNumberOfElements(6);
    extentprop->SetElements(extent);
    maxprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("Maximum"));
    maxprop->SetElements1(255);
    devprop = vtkSMDoubleVectorProperty::SafeDownCast(texture->GetProperty("StandardDeviation"));
    devprop->SetElements1(0.3);
    alphamethodprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("AlphaMethod"));
    alphamethodprop->SetElements1(2);
    alphathresholdprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("AlphaThreshold"));
    alphathresholdprop->SetElements1(63);
    texture->UpdateVTKObjects();

    vtkSMProxyProperty* textureProperty = vtkSMProxyProperty::SafeDownCast(this->GetProperty("Texture"));
    if(textureProperty->GetNumberOfProxies() == 0)
      {
      // set this texture as default texture
      textureProperty->SetProxy(0, texture);
      this->UpdateVTKObjects();
      }
    }

  texName = "Blur";
  created = false;
  proxyIter = vtkSMProxyIterator::New();
  proxyIter->SetModeToOneGroup();
  for (proxyIter->Begin("textures"); !proxyIter->IsAtEnd(); proxyIter->Next())
    {
    string name = proxyIter->GetKey();
    if (name == texName)
      {
      created = true;
      break;
      }
    }

  if (!created)
    {
    // create the texture proxy
    texture = pxm->NewProxy("textures", "SpriteTexture");
    texture->SetConnectionID(this->GetConnectionID());
    texture->SetServers(vtkProcessModule::CLIENT
        | vtkProcessModule::RENDER_SERVER);
    pxm->RegisterProxy("textures", texName.c_str(), texture);

    // set the texture parameters
    extentprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("WholeExtent"));
    extentprop->SetNumberOfElements(6);
    extentprop->SetElements(extent);
    maxprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("Maximum"));
    maxprop->SetElements1(255);
    devprop = vtkSMDoubleVectorProperty::SafeDownCast(texture->GetProperty("StandardDeviation"));
    devprop->SetElements1(0.2);
    alphamethodprop = vtkSMIntVectorProperty::SafeDownCast(texture->GetProperty("AlphaMethod"));
    alphamethodprop->SetElements1(1);

    texture->UpdateVTKObjects();

    texture->Delete();
    }
  proxyIter->Delete();

}


/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVView.h"

#include "vtkInformation.h"
#include "vtkInformationRequestKey.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVDataRepresentation.h"
#include "vtkPVServerInformation.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkRemoteConnection.h"

#include <assert.h>

vtkWeakPointer<vtkPVSynchronizedRenderWindows> vtkPVView::SingletonSynchronizedWindows;

vtkInformationKeyMacro(vtkPVView, REQUEST_UPDATE, Request);
vtkInformationKeyMacro(vtkPVView, REQUEST_INFORMATION, Request);
vtkInformationKeyMacro(vtkPVView, REQUEST_PREPARE_FOR_RENDER, Request);
vtkInformationKeyMacro(vtkPVView, REQUEST_RENDER, Request);
//----------------------------------------------------------------------------
vtkPVView::vtkPVView()
{
  if (vtkPVView::SingletonSynchronizedWindows == NULL)
    {
    this->SynchronizedWindows = vtkPVSynchronizedRenderWindows::New();
    vtkPVView::SingletonSynchronizedWindows = this->SynchronizedWindows;
    }
  else
    {
    this->SynchronizedWindows = vtkPVView::SingletonSynchronizedWindows;
    this->SynchronizedWindows->Register(this);
    }
  this->Identifier = 0;
  this->ViewTime = 0.0;
  this->CacheKey = 0.0;
  this->UseCache = false;

  this->RequestInformation = vtkInformation::New();
  this->ReplyInformationVector = vtkInformationVector::New();
}

//----------------------------------------------------------------------------
vtkPVView::~vtkPVView()
{
  this->SynchronizedWindows->RemoveAllRenderers(this->Identifier);
  this->SynchronizedWindows->RemoveRenderWindow(this->Identifier);
  this->SynchronizedWindows->Delete();
  this->SynchronizedWindows = NULL;

  this->RequestInformation->Delete();
  this->ReplyInformationVector->Delete();
}

//----------------------------------------------------------------------------
void vtkPVView::Initialize(unsigned int id)
{
  assert(this->Identifier == 0 && id != 0);

  this->Identifier = id;
}

//----------------------------------------------------------------------------
void vtkPVView::SetPosition(int x, int y)
{
  assert(this->Identifier != 0);
  this->SynchronizedWindows->SetWindowPosition(this->Identifier, x, y);
}

//----------------------------------------------------------------------------
void vtkPVView::SetSize(int x, int y)
{
  assert(this->Identifier != 0);
  this->SynchronizedWindows->SetWindowSize(this->Identifier, x, y);
}

//----------------------------------------------------------------------------
void vtkPVView::SetViewTime(double time)
{
  if (this->ViewTime != time)
    {
    this->ViewTime = time;
    this->InvokeEvent(ViewTimeChangedEvent);
    this->Modified();
    }
}

//----------------------------------------------------------------------------
bool vtkPVView::InTileDisplayMode()
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkPVServerInformation* info = pm->GetServerInformation(NULL);
  if (info->GetTileDimensions()[0] > 0 ||
    info->GetTileDimensions()[1] > 0)
    {
    return true;
    }
  if (pm->GetActiveRemoteConnection())
    {
    info = pm->GetServerInformation(pm->GetConnectionID(
        pm->GetActiveRemoteConnection()));
    }
  return (info->GetTileDimensions()[0] > 0 ||
    info->GetTileDimensions()[1] > 0);
}

//----------------------------------------------------------------------------
bool vtkPVView::SynchronizeBounds(double bounds[6])
{
  return this->SynchronizedWindows->SynchronizeBounds(bounds);
}

//----------------------------------------------------------------------------
bool vtkPVView::SynchronizeSize(unsigned long &size)
{
  return this->SynchronizedWindows->SynchronizeSize(size);
}

//----------------------------------------------------------------------------
void vtkPVView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Identifier: " << this->Identifier << endl;
  os << indent << "ViewTime: " << this->ViewTime << endl;
  os << indent << "CacheKey: " << this->CacheKey << endl;
  os << indent << "UseCache: " << this->UseCache << endl;
}

//----------------------------------------------------------------------------
void vtkPVView::PrepareForScreenshot()
{
  if (!this->InTileDisplayMode())
    {
    this->SynchronizedWindows->RenderOneViewAtATimeOn();
    }
}

//----------------------------------------------------------------------------
void vtkPVView::CleanupAfterScreenshot()
{
  if (!this->InTileDisplayMode())
    {
    this->SynchronizedWindows->RenderOneViewAtATimeOff();
    }
}

//----------------------------------------------------------------------------
void vtkPVView::Update()
{
  this->CallProcessViewRequest(vtkPVView::REQUEST_UPDATE(),
    this->RequestInformation, this->ReplyInformationVector);
}

//----------------------------------------------------------------------------
void vtkPVView::CallProcessViewRequest(
  vtkInformationRequestKey* type, vtkInformation* inInfo, vtkInformationVector* outVec)
{
  int num_reprs = this->GetNumberOfRepresentations();
  outVec->SetNumberOfInformationObjects(num_reprs);

  if (type == REQUEST_UPDATE())
    {
    // Pass the view time before updating the representations.
    for (int cc=0; cc < num_reprs; cc++)
      {
      vtkDataRepresentation* repr = this->GetRepresentation(cc);
      vtkPVDataRepresentation* pvrepr = vtkPVDataRepresentation::SafeDownCast(repr);
      if (pvrepr)
        {
        // Pass the view time information to the representation
        pvrepr->SetUpdateTime(this->GetViewTime());
        pvrepr->SetUseCache(this->GetUseCache());
        pvrepr->SetCacheKey(this->GetCacheKey());
        }
      }
    }

  for (int cc=0; cc < num_reprs; cc++)
    {
    vtkInformation* outInfo = outVec->GetInformationObject(cc);
    outInfo->Clear();
    vtkDataRepresentation* repr = this->GetRepresentation(cc);
    vtkPVDataRepresentation* pvrepr = vtkPVDataRepresentation::SafeDownCast(repr);
    if (pvrepr)
      {
      pvrepr->ProcessViewRequest(type, inInfo, outInfo);
      }
    else if (repr && type == REQUEST_UPDATE())
      {
      repr->Update();
      }
    }

  // Clear input information since we are done with the pass. This avoids any
  // need for garbage collection.
  inInfo->Clear();
}

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

#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVServerInformation.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkRemoteConnection.h"

#include <assert.h>

vtkWeakPointer<vtkPVSynchronizedRenderWindows> vtkPVView::SingletonSynchronizedWindows;

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
}

//----------------------------------------------------------------------------
vtkPVView::~vtkPVView()
{
  this->SynchronizedWindows->RemoveAllRenderers(this->Identifier);
  this->SynchronizedWindows->RemoveRenderWindow(this->Identifier);
  this->SynchronizedWindows->Delete();
  this->SynchronizedWindows = NULL;
}

//----------------------------------------------------------------------------
void vtkPVView::Initialize(unsigned int id)
{
  assert(this->Identifier == 0 && id != 0);

  this->Identifier = id;
  if (this->SynchronizedWindows->GetRenderWindow(id) == NULL)
    {
    // if the subclass didn't add any render window, we ensure that a NULL is
    // at least assigned so that the various size computations work correctly.
    this->SynchronizedWindows->AddRenderWindow(id, NULL);
    }
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

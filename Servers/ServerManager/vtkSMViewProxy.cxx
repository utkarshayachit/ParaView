/*=========================================================================

  Program:   ParaView
  Module:    vtkSMViewProxy.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMViewProxy.h"

#include "vtkClientServerStream.h"
#include "vtkCommand.h"
#include "vtkImageData.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVXMLElement.h"
#include "vtkSmartPointer.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMRepresentationProxy.h"

vtkStandardNewMacro(vtkSMViewProxy);
//----------------------------------------------------------------------------
vtkSMViewProxy::vtkSMViewProxy()
{
  this->SetServers(vtkProcessModule::CLIENT_AND_SERVERS);
  this->DefaultRepresentationName = 0;
}

//----------------------------------------------------------------------------
vtkSMViewProxy::~vtkSMViewProxy()
{
  this->SetDefaultRepresentationName(0);
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::CreateVTKObjects()
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

  if (!this->GetID().IsNull())
    {
    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "Initialize"
      << static_cast<unsigned int>(this->GetSelfID().ID)
      << vtkClientServerStream::End;
    vtkProcessModule::GetProcessModule()->SendStream(
      this->ConnectionID,
      this->Servers, stream);
    }
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::StillRender()
{
  int interactive = 0;
  this->InvokeEvent(vtkCommand::StartEvent, &interactive);
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;

  // We call update separately from the render. This is done so that we don't
  // get any synchronization issues with GUI responding to the data-updated
  // event by making some data information requests(for example). If those
  // happen while StillRender/InteractiveRender is being executed on the server
  // side then we get deadlocks.
  this->Update();

  if (!this->GetID().IsNull())
    {
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "StillRender"
      << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, this->Servers, stream);
    }
  this->PostRender(interactive==1);
  this->InvokeEvent(vtkCommand::EndEvent, &interactive);
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::InteractiveRender()
{
  int interactive = 1;
  this->InvokeEvent(vtkCommand::StartEvent, &interactive);

  // We call update separately from the render. This is done so that we don't
  // get any synchronization issues with GUI responding to the data-updated
  // event by making some data information requests(for example). If those
  // happen while StillRender/InteractiveRender is being executed on the server
  // side then we get deadlocks.
  this->Update();

  if (!this->GetID().IsNull())
    {
    vtkClientServerStream stream;
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "InteractiveRender"
      << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, this->Servers, stream);
    }

  this->PostRender(interactive==1);
  this->InvokeEvent(vtkCommand::EndEvent, &interactive);
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::Update()
{
  if (!this->GetID().IsNull())
    {
    vtkClientServerStream stream;
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "Update"
      << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, this->Servers, stream);
    }
}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy* vtkSMViewProxy::CreateDefaultRepresentation(
  vtkSMProxy* vtkNotUsed(proxy), int vtkNotUsed(opport))
{
  if (this->DefaultRepresentationName)
    {
    vtkSMProxyManager* pxm = vtkSMObject::GetProxyManager();
    vtkSmartPointer<vtkSMProxy> p;
    p.TakeReference(pxm->NewProxy("representations", this->DefaultRepresentationName));
    vtkSMRepresentationProxy* repr = vtkSMRepresentationProxy::SafeDownCast(p);
    if (repr)
      {
      repr->Register(this);
      return repr;
      }
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkSMViewProxy::ReadXMLAttributes(
  vtkSMProxyManager* pm, vtkPVXMLElement* element)
{
  if (!this->Superclass::ReadXMLAttributes(pm, element))
    {
    return 0;
    }

  const char* repr_name = element->GetAttribute("representation_name");
  if (repr_name)
    {
    this->SetDefaultRepresentationName(repr_name);
    }
  return 1;
}

//----------------------------------------------------------------------------
vtkImageData* vtkSMViewProxy::CaptureWindow(int magnification)
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  if (!this->GetID().IsNull())
    {
    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "PrepareForScreenshot"
      << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, this->Servers, stream);
    }

  vtkImageData* capture = this->CaptureWindowInternal(magnification);

  if (!this->GetID().IsNull())
    {
    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "CleanupAfterScreenshot"
      << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, this->Servers, stream);
    }

  if (capture)
    {
    int position[2];
    vtkSMPropertyHelper(this, "ViewPosition").Get(position, 2);

    // Update image extents based on ViewPosition
    int extents[6];
    capture->GetExtent(extents);
    for (int cc=0; cc < 4; cc++)
      {
      extents[cc] += position[cc/2]*magnification;
      }
    capture->SetExtent(extents);
    }
  return capture;
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}



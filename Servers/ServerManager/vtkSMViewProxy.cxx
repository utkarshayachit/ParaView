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
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"

#include <vtkstd/vector>

vtkStandardNewMacro(vtkSMViewProxy);
//----------------------------------------------------------------------------
vtkSMViewProxy::vtkSMViewProxy()
{
  this->SetServers(vtkProcessModule::CLIENT_AND_SERVERS);
}

//----------------------------------------------------------------------------
vtkSMViewProxy::~vtkSMViewProxy()
{
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::StillRender()
{
  int interactive = 0;
  this->InvokeEvent(vtkCommand::StartEvent, &interactive);
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
    << this->GetID()
    << "StillRender"
    << vtkClientServerStream::End;
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  pm->SendStream(this->ConnectionID, this->Servers, stream);
  this->InvokeEvent(vtkCommand::EndEvent, &interactive);
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::InteractiveRender()
{
  int interactive = 1;
  this->InvokeEvent(vtkCommand::StartEvent, &interactive);
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
    << this->GetID()
    << "InteractiveRender"
    << vtkClientServerStream::End;
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  pm->SendStream(this->ConnectionID, this->Servers, stream);
  this->InvokeEvent(vtkCommand::EndEvent, &interactive);
}

//----------------------------------------------------------------------------
void vtkSMViewProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}



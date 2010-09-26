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
#include "vtkSMRepresentationProxy.h"

#include "vtkClientServerStream.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkMemberFunctionCommand.h"

vtkStandardNewMacro(vtkSMRepresentationProxy);
//----------------------------------------------------------------------------
vtkSMRepresentationProxy::vtkSMRepresentationProxy()
{

}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy::~vtkSMRepresentationProxy()
{
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }
  this->Superclass::CreateVTKObjects();

  vtkMemberFunctionCommand<vtkSMRepresentationProxy>* observer =
    vtkMemberFunctionCommand<vtkSMRepresentationProxy>::New();
  observer->SetCallback(*this, &vtkSMRepresentationProxy::RepresentationUpdated);

  vtkObject::SafeDownCast(this->GetClientSideObject())->AddObserver(
    vtkCommand::UpdateDataEvent, observer);
  observer->Delete();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::MarkDirty(vtkSMProxy* modifiedProxy)
{
  cout << "MarkModified" << endl;
  if (modifiedProxy != this && this->ObjectsCreated && !this->NeedsUpdate)
    {
    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
      << this->GetID()
      << "MarkModified"
      << vtkClientServerStream::End;
    vtkProcessModule::GetProcessModule()->SendStream(
      this->ConnectionID,
      this->Servers, stream);
    }
  this->Superclass::MarkModified(modifiedProxy);
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::RepresentationUpdated()
{
  cout << "RepresentationUpdated" << endl;
  this->PostUpdateData();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

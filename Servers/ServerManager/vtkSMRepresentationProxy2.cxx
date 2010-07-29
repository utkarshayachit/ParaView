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
#include "vtkSMRepresentationProxy2.h"

#include "vtkClientServerStream.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkMemberFunctionCommand.h"

vtkStandardNewMacro(vtkSMRepresentationProxy2);
//----------------------------------------------------------------------------
vtkSMRepresentationProxy2::vtkSMRepresentationProxy2()
{

}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy2::~vtkSMRepresentationProxy2()
{
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy2::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }
  this->Superclass::CreateVTKObjects();

  vtkMemberFunctionCommand<vtkSMRepresentationProxy2>* observer =
    vtkMemberFunctionCommand<vtkSMRepresentationProxy2>::New();
  observer->SetCallback(*this, &vtkSMRepresentationProxy2::RepresentationUpdated);

  vtkObject::SafeDownCast(this->GetClientSideObject())->AddObserver(
    vtkCommand::UpdateDataEvent, observer);
  observer->Delete();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy2::MarkDirty(vtkSMProxy* modifiedProxy)
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
void vtkSMRepresentationProxy2::RepresentationUpdated()
{
  cout << "RepresentationUpdated" << endl;
  this->PostUpdateData();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy2::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

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
#include "vtkMemberFunctionCommand.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVRepresentedDataInformation.h"
#include "vtkTimerLog.h"

vtkStandardNewMacro(vtkSMRepresentationProxy);
//----------------------------------------------------------------------------
vtkSMRepresentationProxy::vtkSMRepresentationProxy()
{
  this->RepresentedDataInformationValid = false;
  this->RepresentedDataInformation = vtkPVRepresentedDataInformation::New();
  this->MarkedModified = false;
}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy::~vtkSMRepresentationProxy()
{
  this->RepresentedDataInformation->Delete();
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
void vtkSMRepresentationProxy::UpdatePipeline()
{
  if (!this->NeedsUpdate)
    {
    return;
    }

  this->UpdatePipelineInternal(0, false);
  this->Superclass::UpdatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::UpdatePipeline(double time)
{
  this->UpdatePipelineInternal(time, true);
  this->Superclass::UpdatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::UpdatePipelineInternal(
  double time, bool doTime)
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;
#ifdef FIXME
  stream << vtkClientServerStream::Invoke
         << this->GetProducerID() << "UpdateInformation"
         << vtkClientServerStream::End;

  stream << vtkClientServerStream::Invoke
         << pm->GetProcessModuleID() << "GetPartitionId"
         << vtkClientServerStream::End
         << vtkClientServerStream::Invoke
         << this->GetExecutiveID() << "SetUpdateExtent" << this->PortIndex
         << vtkClientServerStream::LastResult
         << pm->GetNumberOfPartitions(this->ConnectionID) << 0
         << vtkClientServerStream::End;
  if (doTime)
    {
    stream << vtkClientServerStream::Invoke
           << this->GetExecutiveID() << "SetUpdateTimeStep"
           << this->PortIndex << time
           << vtkClientServerStream::End;
    }
#endif
  stream << vtkClientServerStream::Invoke
         << this->GetID() << "Update"
         << vtkClientServerStream::End;
  pm->SendPrepareProgress(this->ConnectionID);
  pm->SendStream(this->ConnectionID, this->Servers, stream);
  pm->SendCleanupPendingProgress(this->ConnectionID);
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::MarkDirty(vtkSMProxy* modifiedProxy)
{
  if ((modifiedProxy != this) && this->ObjectsCreated)
    {
    if (!this->MarkedModified)
      {
      cout << "MarkModified" << endl;
      this->MarkedModified = true;
      vtkClientServerStream stream;
      stream << vtkClientServerStream::Invoke
        << this->GetID()
        << "MarkModified"
        << vtkClientServerStream::End;
      vtkProcessModule::GetProcessModule()->SendStream(
        this->ConnectionID,
        this->Servers, stream);
      }
    }
  this->Superclass::MarkModified(modifiedProxy);
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::RepresentationUpdated()
{
  cout << "RepresentationUpdated" << endl;
  this->MarkedModified = false;
  this->PostUpdateData();
  // PostUpdateData will call InvalidateDataInformation() which will mark
  // RepresentedDataInformationValid as false;
  // this->RepresentedDataInformationValid = false;
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::InvalidateDataInformation()
{
  this->Superclass::InvalidateDataInformation();
  this->RepresentedDataInformationValid = false;
}

//----------------------------------------------------------------------------
vtkPVDataInformation* vtkSMRepresentationProxy::GetRepresentedDataInformation()
{
  if (!this->RepresentedDataInformationValid)
    {
    vtkTimerLog::MarkStartEvent(
      "vtkSMRepresentationProxy::GetRepresentedDataInformation");
    this->RepresentedDataInformation->Initialize();
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    pm->GatherInformation(this->ConnectionID, this->Servers,
      this->RepresentedDataInformation, this->GetID());
    vtkTimerLog::MarkEndEvent(
      "vtkSMRepresentationProxy::GetRepresentedDataInformation");
    this->RepresentedDataInformationValid = true;
    }

  return this->RepresentedDataInformation;
}

//----------------------------------------------------------------------------
void vtkSMRepresentationProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

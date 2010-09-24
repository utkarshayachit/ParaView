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
#include "vtkSMRenderView2Proxy.h"

#include "vtkClientServerStream.h"
#include "vtkCollection.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkPVLastSelectionInformation.h"
#include "vtkPVRenderView.h"
#include "vtkPVRenderViewProxy.h"
#include "vtkSmartPointer.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSelectionHelper.h"
#include "vtkWeakPointer.h"

namespace
{
  class vtkRenderHelper : public vtkPVRenderViewProxy
  {
public:
  static vtkRenderHelper* New();
  vtkTypeMacro(vtkRenderHelper, vtkPVRenderViewProxy);

  virtual void EventuallyRender()
    {
    this->Proxy->InvokeCommand("StillRender");
    }
  virtual vtkRenderWindow* GetRenderWindow() { return NULL; }
  virtual void Render()
    {
    this->Proxy->InvokeCommand("InteractiveRender");
    }

  vtkWeakPointer<vtkSMProxy> Proxy;
  };
  vtkStandardNewMacro(vtkRenderHelper);
};

vtkStandardNewMacro(vtkSMRenderView2Proxy);
vtkCxxRevisionMacro(vtkSMRenderView2Proxy, "$Revision$");
//----------------------------------------------------------------------------
vtkSMRenderView2Proxy::vtkSMRenderView2Proxy()
{
}

//----------------------------------------------------------------------------
vtkSMRenderView2Proxy::~vtkSMRenderView2Proxy()
{
}

//----------------------------------------------------------------------------
void vtkSMRenderView2Proxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }
  this->Superclass::CreateVTKObjects();

  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << this->GetID()
         << "Initialize"
         << static_cast<unsigned int>(this->GetSelfID().ID)
         << vtkClientServerStream::End;

  vtkProcessModule::GetProcessModule()->SendStream(
    this->ConnectionID,
    this->Servers, stream);

  vtkPVRenderView* rv = vtkPVRenderView::SafeDownCast(
    this->GetClientSideObject());
  if (rv->GetInteractor())
    {
    vtkRenderHelper* helper = vtkRenderHelper::New();
    helper->Proxy = this;
    rv->GetInteractor()->SetPVRenderView(helper);
    helper->Delete();

    vtkCommand *obs = vtkMakeMemberFunctionCommand(*this,
      &vtkSMRenderView2Proxy::OnSelect);
    rv->AddObserver(vtkCommand::SelectionChangedEvent,
      obs);
    obs->Delete();
    }
}

//----------------------------------------------------------------------------
bool vtkSMRenderView2Proxy::SelectSurfaceCells(int region[4],
  vtkCollection* selectedRepresentations,
  vtkCollection* selectionSources,
  bool vtkNotUsed(multiple_selections))
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
    << this->GetID()
    << "SelectCells"
    << vtkClientServerStream::InsertArray(region, 4)
    << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, this->Servers, stream);

  return this->FetchLastSelection(selectedRepresentations, selectionSources);
}

//----------------------------------------------------------------------------
bool vtkSMRenderView2Proxy::SelectSurfacePoints(int region[4],
  vtkCollection* selectedRepresentations,
  vtkCollection* selectionSources,
  bool vtkNotUsed(multiple_selections))
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
    << this->GetID()
    << "SelectPoints"
    << vtkClientServerStream::InsertArray(region, 4)
    << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, this->Servers, stream);

  return this->FetchLastSelection(selectedRepresentations, selectionSources);
}

//----------------------------------------------------------------------------
bool vtkSMRenderView2Proxy::FetchLastSelection(
  vtkCollection* selectedRepresentations, vtkCollection* selectionSources)
{
  if (selectionSources && selectedRepresentations)
    {
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    vtkSmartPointer<vtkPVLastSelectionInformation> info =
      vtkSmartPointer<vtkPVLastSelectionInformation>::New();
    pm->GatherInformation(this->ConnectionID,
      vtkProcessModule::DATA_SERVER, info, this->GetID());

    vtkSelection* selection = info->GetSelection();
    vtkSMSelectionHelper::NewSelectionSourcesFromSelection(
      selection, this, selectionSources, selectedRepresentations);
    return (selectionSources->GetNumberOfItems() > 0);
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkSMRenderView2Proxy::OnSelect(vtkObject*, unsigned long, void* vregion)
{
  int *region = reinterpret_cast<int*>(vregion);
  this->SelectSurfaceCells(region, NULL, NULL);
}

//----------------------------------------------------------------------------
void vtkSMRenderView2Proxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

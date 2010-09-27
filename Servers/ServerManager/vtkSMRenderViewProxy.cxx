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
#include "vtkSMRenderViewProxy.h"

#include "vtkClientServerStream.h"
#include "vtkCollection.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkPVLastSelectionInformation.h"
#include "vtkPVRenderView.h"
#include "vtkPVRenderViewProxy.h"
#include "vtkPVXMLElement.h"
#include "vtkSmartPointer.h"
#include "vtkSMInputProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMSelectionHelper.h"
#include "vtkSMSourceProxy.h"
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
    this->Proxy->StillRender();
    }
  virtual vtkRenderWindow* GetRenderWindow() { return NULL; }
  virtual void Render()
    {
    this->Proxy->InteractiveRender();
    }

  vtkWeakPointer<vtkSMRenderViewProxy> Proxy;
  };
  vtkStandardNewMacro(vtkRenderHelper);
};

vtkStandardNewMacro(vtkSMRenderViewProxy);
vtkCxxRevisionMacro(vtkSMRenderViewProxy, "$Revision$");
//----------------------------------------------------------------------------
vtkSMRenderViewProxy::vtkSMRenderViewProxy()
{
}

//----------------------------------------------------------------------------
vtkSMRenderViewProxy::~vtkSMRenderViewProxy()
{
}

//----------------------------------------------------------------------------
vtkRenderWindow* vtkSMRenderViewProxy::GetRenderWindow()
{
  this->CreateVTKObjects();
  vtkPVRenderView* rv = vtkPVRenderView::SafeDownCast(
    this->GetClientSideObject());
  return rv? rv->GetRenderWindow() : NULL;
}

//----------------------------------------------------------------------------
vtkRenderer* vtkSMRenderViewProxy::GetRenderer()
{
  this->CreateVTKObjects();
  vtkPVRenderView* rv = vtkPVRenderView::SafeDownCast(
    this->GetClientSideObject());
  return rv? rv->GetRenderer() : NULL;
}

//----------------------------------------------------------------------------
vtkPVGenericRenderWindowInteractor* vtkSMRenderViewProxy::GetInteractor()
{
  this->CreateVTKObjects();
  vtkPVRenderView* rv = vtkPVRenderView::SafeDownCast(
    this->GetClientSideObject());
  return rv? rv->GetInteractor() : NULL;
}

//----------------------------------------------------------------------------
void vtkSMRenderViewProxy::CreateVTKObjects()
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

  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << this->GetID()
         << "Initialize"
         << static_cast<unsigned int>(this->GetSelfID().ID)
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->GetID()
         << "SetActiveCamera"
         << this->GetSubProxy("ActiveCamera")->GetID()
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
      &vtkSMRenderViewProxy::OnSelect);
    rv->AddObserver(vtkCommand::SelectionChangedEvent,
      obs);
    obs->Delete();
    }
}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy* vtkSMRenderViewProxy::CreateDefaultRepresentation(
  vtkSMProxy* source, int opport)
{
  if (!source)
    {
    return 0;
    }

  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();

  // Update with time to avoid domains updating without time later.
  vtkSMSourceProxy* sproxy = vtkSMSourceProxy::SafeDownCast(source);
  if (sproxy)
    {
    double view_time = vtkSMPropertyHelper(this, "ViewTime").GetAsDouble();
    sproxy->UpdatePipeline(view_time);
    }

  // Choose which type of representation proxy to create.
  vtkSMProxy* prototype = pxm->GetPrototypeProxy("new_representations",
    "UnstructuredGridRepresentation");

  vtkSMInputProperty* pp = vtkSMInputProperty::SafeDownCast(
    prototype->GetProperty("Input"));
  pp->RemoveAllUncheckedProxies();
  pp->AddUncheckedInputConnection(source, opport);
  bool usg = (pp->IsInDomains()>0);
  pp->RemoveAllUncheckedProxies();
  if (usg)
    {
    return vtkSMRepresentationProxy::SafeDownCast(
      pxm->NewProxy("new_representations", "UnstructuredGridRepresentation"));
    }

  prototype = pxm->GetPrototypeProxy("representations",
    "UniformGridRepresentation");
  pp = vtkSMInputProperty::SafeDownCast(
    prototype->GetProperty("Input"));
  pp->RemoveAllUncheckedProxies();
  pp->AddUncheckedInputConnection(source, opport);
  bool sg = (pp->IsInDomains()>0);
  pp->RemoveAllUncheckedProxies();
  if (sg)
    {
    return vtkSMRepresentationProxy::SafeDownCast(
      pxm->NewProxy("new_representations", "UniformGridRepresentation"));
    }

  prototype = pxm->GetPrototypeProxy("new_representations",
    "GeometryRepresentation");
  pp = vtkSMInputProperty::SafeDownCast(
    prototype->GetProperty("Input"));
  pp->RemoveAllUncheckedProxies();
  pp->AddUncheckedInputConnection(source, opport);
  bool g = (pp->IsInDomains()>0);
  pp->RemoveAllUncheckedProxies();
  if (g)
    {
    return vtkSMRepresentationProxy::SafeDownCast(
      pxm->NewProxy("new_representations", "GeometryRepresentation"));
    }

  vtkPVXMLElement* hints = source->GetHints();
  if (hints)
    {
    // If the source has an hint as follows, then it's a text producer and must
    // be is display-able.
    //  <Hints>
    //    <OutputPort name="..." index="..." type="text" />
    //  </Hints>

    unsigned int numElems = hints->GetNumberOfNestedElements();
    for (unsigned int cc=0; cc < numElems; cc++)
      {
      int index;
      vtkPVXMLElement* child = hints->GetNestedElement(cc);
      if (child->GetName() &&
        strcmp(child->GetName(), "OutputPort") == 0 &&
        child->GetScalarAttribute("index", &index) &&
        index == opport &&
        child->GetAttribute("type") &&
        strcmp(child->GetAttribute("type"), "text") == 0)
        {
        return vtkSMRepresentationProxy::SafeDownCast(
          pxm->NewProxy("representations", "TextSourceRepresentation"));
        }
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
bool vtkSMRenderViewProxy::SelectSurfaceCells(int region[4],
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
bool vtkSMRenderViewProxy::SelectSurfacePoints(int region[4],
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
bool vtkSMRenderViewProxy::FetchLastSelection(
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
void vtkSMRenderViewProxy::OnSelect(vtkObject*, unsigned long, void* vregion)
{
  int *region = reinterpret_cast<int*>(vregion);
  this->SelectSurfaceCells(region, NULL, NULL);
}

//----------------------------------------------------------------------------
void vtkSMRenderViewProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

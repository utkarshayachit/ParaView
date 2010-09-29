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
#include "vtkEventForwarderCommand.h"
#include "vtkExtractSelectedFrustum.h"
#include "vtkImageData.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVDataInformation.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkPVLastSelectionInformation.h"
#include "vtkPVRenderView.h"
#include "vtkPVRenderViewProxy.h"
#include "vtkPVXMLElement.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkSelectionNode.h"
#include "vtkSmartPointer.h"
#include "vtkSMInputProperty.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxyManager.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMSelectionHelper.h"
#include "vtkWeakPointer.h"
#include "vtkWindowToImageFilter.h"

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
  this->UseInteractiveRenderingForSceenshots = false;
}

//----------------------------------------------------------------------------
vtkSMRenderViewProxy::~vtkSMRenderViewProxy()
{
}

//-----------------------------------------------------------------------------
bool vtkSMRenderViewProxy::IsSelectionAvailable()
{
  const char* msg = this->IsSelectVisibleCellsAvailable();
  if (msg)
    {
    vtkErrorMacro(<< msg);
    return false;
    }

  return true;
}

//-----------------------------------------------------------------------------
const char* vtkSMRenderViewProxy::IsSelectVisibleCellsAvailable()
{
  //check if we don't have enough color depth to do color buffer selection
  //if we don't then disallow selection
  int rgba[4];
  vtkRenderWindow *rwin = this->GetRenderWindow();
  if (!rwin)
    {
    return "No render window available";
    }

  rwin->GetColorBufferSizes(rgba);
  if (rgba[0] < 8 || rgba[1] < 8 || rgba[2] < 8)
    {
    return "Selection not supported due to insufficient color depth.";
    }

  //cout << "yes" << endl;
  return NULL;
}

//-----------------------------------------------------------------------------
const char* vtkSMRenderViewProxy::IsSelectVisiblePointsAvailable()
{
  return this->IsSelectVisibleCellsAvailable();
}

//-----------------------------------------------------------------------------
void vtkSMRenderViewProxy::PostRender(bool interactive)
{
  vtkSMProxy* cameraProxy = this->GetSubProxy("ActiveCamera");
  cameraProxy->UpdatePropertyInformation();
  this->Superclass::PostRender(interactive);
}

//-----------------------------------------------------------------------------
void vtkSMRenderViewProxy::SynchronizeCameraProperties()
{
  if (!this->ObjectsCreated)
    {
    return;
    }

  vtkSMProxy* cameraProxy = this->GetSubProxy("ActiveCamera");
  cameraProxy->UpdatePropertyInformation();
  vtkSMPropertyIterator* iter = cameraProxy->NewPropertyIterator();
  for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
    vtkSMProperty *cur_property = iter->GetProperty();
    vtkSMProperty *info_property = cur_property->GetInformationProperty();
    if (!info_property)
      {
      continue;
      }
    cur_property->Copy(info_property);
    //cur_property->UpdateLastPushedValues();
    }
  iter->Delete();
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
vtkCamera* vtkSMRenderViewProxy::GetActiveCamera()
{
  this->CreateVTKObjects();
  vtkPVRenderView* rv = vtkPVRenderView::SafeDownCast(
    this->GetClientSideObject());
  return rv? rv->GetActiveCamera() : NULL;
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
    }

  vtkEventForwarderCommand* forwarder = vtkEventForwarderCommand::New();
  forwarder->SetTarget(this);
  rv->AddObserver(vtkCommand::SelectionChangedEvent, forwarder);
  rv->AddObserver(vtkCommand::ResetCameraEvent, forwarder);
  forwarder->Delete();
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
  vtkSMProxy* prototype = pxm->GetPrototypeProxy("representations",
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
      pxm->NewProxy("representations", "UnstructuredGridRepresentation"));
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
      pxm->NewProxy("representations", "UniformGridRepresentation"));
    }

  prototype = pxm->GetPrototypeProxy("representations",
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
      pxm->NewProxy("representations", "GeometryRepresentation"));
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
  if (!this->IsSelectionAvailable())
    {
    return false;
    }

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
  if (!this->IsSelectionAvailable())
    {
    return false;
    }

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
bool vtkSMRenderViewProxy::SelectFrustumCells(int region[4],
  vtkCollection* selectedRepresentations,
  vtkCollection* selectionSources,
  bool multiple_selections)
{
  return this->SelectFrustumInternal(region, selectedRepresentations,
    selectionSources, multiple_selections, vtkSelectionNode::CELL);
}

//----------------------------------------------------------------------------
bool vtkSMRenderViewProxy::SelectFrustumPoints(int region[4],
  vtkCollection* selectedRepresentations,
  vtkCollection* selectionSources,
  bool multiple_selections)
{
  return this->SelectFrustumInternal(region, selectedRepresentations,
    selectionSources, multiple_selections, vtkSelectionNode::POINT);
}

//----------------------------------------------------------------------------
bool vtkSMRenderViewProxy::SelectFrustumInternal(int region[4],
  vtkCollection* selectedRepresentations,
  vtkCollection* selectionSources,
  bool multiple_selections,
  int fieldAssociation)
{
  // Simply stealing old code for now. This code have many coding style
  // violations and seems too long for what it does. At some point we'll check
  // it out.

  int displayRectangle[4] = {region[0], region[1], region[2], region[3]};
  if (displayRectangle[0] == displayRectangle[2])
    {
    displayRectangle[2] += 1;
    }
  if (displayRectangle[1] == displayRectangle[3])
    {
    displayRectangle[3] += 1;
    }

  // 1) Create frustum selection
  //convert screen rectangle to world frustum
  vtkRenderer *renderer = this->GetRenderer();
  double frustum[32];
  int index=0;
  renderer->SetDisplayPoint(displayRectangle[0], displayRectangle[1], 0);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[0], displayRectangle[1], 1);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[0], displayRectangle[3], 0);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[0], displayRectangle[3], 1);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[2], displayRectangle[1], 0);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[2], displayRectangle[1], 1);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[2], displayRectangle[3], 0);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);
  index++;
  renderer->SetDisplayPoint(displayRectangle[2], displayRectangle[3], 1);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(&frustum[index*4]);

  vtkSMProxy* selectionSource = this->GetProxyManager()->NewProxy("sources",
    "FrustumSelectionSource");
  selectionSource->SetConnectionID(this->ConnectionID);
  vtkSMPropertyHelper(selectionSource, "FieldType").Set(fieldAssociation);
  vtkSMPropertyHelper(selectionSource, "Frustum").Set(frustum, 32);
  selectionSource->UpdateVTKObjects();

  // 2) Figure out which representation is "selected".
  vtkExtractSelectedFrustum* extractor =
    vtkExtractSelectedFrustum::New();
  extractor->CreateFrustum(frustum);

  // Now we just use the first selected representation,
  // until we have other mechanisms to select one.
  vtkSMPropertyHelper reprsHelper(this, "Representations");

  for (unsigned int cc=0;  cc < reprsHelper.GetNumberOfElements(); cc++)
    {
    vtkSMRepresentationProxy* repr =
      vtkSMRepresentationProxy::SafeDownCast(reprsHelper.GetAsProxy(cc));
    if (!repr || vtkSMPropertyHelper(repr, "Visibility", true).GetAsInt() == 0)
      {
      continue;
      }
    if (vtkSMPropertyHelper(repr, "Pickable", true).GetAsInt() == 0)
      {
      // skip non-pickable representations.
      continue;
      }
    vtkPVDataInformation* datainfo = repr->GetRepresentedDataInformation();
    if (!datainfo)
      {
      continue;
      }

    double bounds[6];
    datainfo->GetBounds(bounds);

    if (extractor->OverallBoundsTest(bounds))
      {
      selectionSources->AddItem(selectionSource);
      selectedRepresentations->AddItem(repr);
      if (!multiple_selections)
        {
        break;
        }
      }
    }

  extractor->Delete();
  selectionSource->Delete();
  return true;
}

//----------------------------------------------------------------------------
vtkImageData* vtkSMRenderViewProxy::CaptureWindowInternal(int magnification)
{
  //this->GetRenderWindow()->SwapBuffersOff();

  if (this->UseInteractiveRenderingForSceenshots)
    {
    this->InteractiveRender();
    }
  else
    {
    this->StillRender();
    }

  vtkWindowToImageFilter* w2i = vtkWindowToImageFilter::New();
  w2i->SetInput(this->GetRenderWindow());
  w2i->SetMagnification(magnification);
  w2i->ReadFrontBufferOff();
  w2i->ReadFrontBufferOn(); // FIXME
  w2i->ShouldRerenderOff();

  // BUG #8715: We go through this indirection since the active connection needs
  // to be set during update since it may request re-renders if magnification >1.
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << w2i << "Update"
         << vtkClientServerStream::End;
  vtkProcessModule::GetProcessModule()->SendStream(
    this->ConnectionID, vtkProcessModule::CLIENT, stream);

  vtkImageData* capture = vtkImageData::New();
  capture->ShallowCopy(w2i->GetOutput());
  w2i->Delete();

  //this->GetRenderWindow()->SwapBuffersOn();
  //this->GetRenderWindow()->Frame();
  return capture;
}

//----------------------------------------------------------------------------
void vtkSMRenderViewProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "UseInteractiveRenderingForSceenshots: " <<
    this->UseInteractiveRenderingForSceenshots << endl;
}

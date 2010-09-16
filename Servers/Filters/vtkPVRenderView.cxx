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
#include "vtkPVRenderView.h"

#include "vtkBSPCutsGenerator.h"
#include "vtkCommand.h"
#include "vtkInformationDoubleKey.h"
#include "vtkInformation.h"
#include "vtkInformationIntegerKey.h"
#include "vtkInformationObjectBaseKey.h"
#include "vtkInformationRequestKey.h"
#include "vtkInformationVector.h"
#include "vtkLight.h"
#include "vtkLightKit.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkMPIMoveData.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPKdTree.h"
#include "vtkProcessModule.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkPVInteractorStyle.h"
#include "vtkPVOptions.h"
#include "vtkPVSynchronizedRenderer.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkRenderer.h"
#include "vtkRenderViewBase.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkWeakPointer.h"

#include "vtkLookupTable.h"
#include "vtkScalarBarActor.h"
#include "vtkPVTrackballRotate.h"

#include <assert.h>
#include <vtkstd/set>

namespace
{
  //-----------------------------------------------------------------------------
  void Create2DPipeline(vtkRenderer* ren)
    {
    vtkScalarBarActor* repr = vtkScalarBarActor::New();
    vtkLookupTable* lut = vtkLookupTable::New();
    lut->Build();
    repr->SetLookupTable(lut);
    lut->Delete();
    ren->AddActor(repr);
    repr->Delete();
    }
};

vtkStandardNewMacro(vtkPVRenderView);
vtkInformationKeyMacro(vtkPVRenderView, GEOMETRY_SIZE, Integer);
vtkInformationKeyMacro(vtkPVRenderView, DATA_DISTRIBUTION_MODE, Integer);
vtkInformationKeyMacro(vtkPVRenderView, USE_LOD, Integer);
vtkInformationKeyMacro(vtkPVRenderView, DELIVER_OUTLINE_TO_CLIENT, Integer);
vtkInformationKeyMacro(vtkPVRenderView, DELIVER_LOD_TO_CLIENT, Integer);
vtkInformationKeyMacro(vtkPVRenderView, LOD_RESOLUTION, Double);
vtkInformationKeyMacro(vtkPVRenderView, NEED_ORDERED_COMPOSITING, Integer);
vtkInformationKeyMacro(vtkPVRenderView, REDISTRIBUTABLE_DATA_PRODUCER, ObjectBase);
vtkInformationKeyMacro(vtkPVRenderView, KD_TREE, ObjectBase);
//----------------------------------------------------------------------------
vtkPVRenderView::vtkPVRenderView()
{
  this->StillRenderImageReductionFactor = 1;
  this->InteractiveRenderImageReductionFactor = 2;
  this->GeometrySize = 0;
  this->RemoteRenderingThreshold = 0;
  this->LODRenderingThreshold = 0;
  this->ClientOutlineThreshold = 100;
  this->LODResolution = 0.5;
  this->UseLightKit = false;
  this->Interactor = 0;
  this->InteractorStyle = 0;

  this->SynchronizedRenderers = vtkPVSynchronizedRenderer::New();

  if (this->SynchronizedWindows->GetLocalProcessIsDriver())
    {
    this->Interactor = vtkPVGenericRenderWindowInteractor::New();
    }

  vtkRenderWindow* window = this->SynchronizedWindows->NewRenderWindow();
  window->SetMultiSamples(0);
  this->RenderView = vtkRenderViewBase::New();
  if (this->Interactor)
    {
    this->RenderView->SetInteractor(this->Interactor);
    }
  this->RenderView->SetRenderWindow(window);
  window->Delete();

  this->NonCompositedRenderer = vtkRenderer::New();
  this->NonCompositedRenderer->EraseOff();
  this->NonCompositedRenderer->InteractiveOff();
  this->NonCompositedRenderer->SetLayer(2);
  this->NonCompositedRenderer->SetActiveCamera(
    this->RenderView->GetRenderer()->GetActiveCamera());
  window->AddRenderer(this->NonCompositedRenderer);
  window->SetNumberOfLayers(3);

  ::Create2DPipeline(this->NonCompositedRenderer);

  vtkMemberFunctionCommand<vtkPVRenderView>* observer =
    vtkMemberFunctionCommand<vtkPVRenderView>::New();
  observer->SetCallback(*this, &vtkPVRenderView::ResetCameraClippingRange);
  this->GetRenderer()->AddObserver(vtkCommand::ResetCameraClippingRangeEvent,
    observer);
  observer->FastDelete();

  this->GetRenderer()->SetUseDepthPeeling(1);

  this->Light = vtkLight::New();
  this->Light->SetAmbientColor(1, 1, 1);
  this->Light->SetSpecularColor(1, 1, 1);
  this->Light->SetDiffuseColor(1, 1, 1);
  this->Light->SetIntensity(1.0);
  this->Light->SetLightType(2); // CameraLight
  this->LightKit = vtkLightKit::New();
  this->GetRenderer()->AddLight(this->Light);

  this->OrderedCompositingBSPCutsSource = vtkBSPCutsGenerator::New();

  if (this->Interactor)
    {
    this->InteractorStyle = vtkPVInteractorStyle::New();
    this->Interactor->SetRenderer(this->GetRenderer());
    this->Interactor->SetRenderWindow(this->GetRenderWindow());
    this->Interactor->SetInteractorStyle(this->InteractorStyle);
    vtkPVTrackballRotate* manip = vtkPVTrackballRotate::New();
    manip->SetButton(1);
    this->InteractorStyle->AddManipulator(manip);
    manip->Delete();
    }
}

//----------------------------------------------------------------------------
vtkPVRenderView::~vtkPVRenderView()
{
  this->SynchronizedRenderers->Delete();
  this->NonCompositedRenderer->Delete();
  this->RenderView->Delete();
  this->LightKit->Delete();
  this->Light->Delete();

  if (this->Interactor)
    {
    this->Interactor->Delete();
    this->Interactor = 0;
    }
  if (this->InteractorStyle)
    {
    this->InteractorStyle->Delete();
    this->InteractorStyle = 0;
    }

  this->OrderedCompositingBSPCutsSource->Delete();
  this->OrderedCompositingBSPCutsSource = NULL;
}

//----------------------------------------------------------------------------
void vtkPVRenderView::Initialize(unsigned int id)
{
  this->SynchronizedWindows->AddRenderWindow(id, this->RenderView->GetRenderWindow());
  this->SynchronizedWindows->AddRenderer(id, this->RenderView->GetRenderer());
  this->SynchronizedWindows->AddRenderer(id, this->GetNonCompositedRenderer());
  this->SynchronizedRenderers->SetRenderer(this->RenderView->GetRenderer());

  this->Superclass::Initialize(id);
}

//----------------------------------------------------------------------------
vtkRenderer* vtkPVRenderView::GetRenderer()
{
  return this->RenderView->GetRenderer();
}

//----------------------------------------------------------------------------
vtkCamera* vtkPVRenderView::GetActiveCamera()
{
  return this->RenderView->GetRenderer()->GetActiveCamera();
}

//----------------------------------------------------------------------------
vtkRenderWindow* vtkPVRenderView::GetRenderWindow()
{
  return this->RenderView->GetRenderWindow();
}

//----------------------------------------------------------------------------
void vtkPVRenderView::ResetCameraClippingRange()
{
  this->GetRenderer()->ResetCameraClippingRange(this->LastComputedBounds);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::GatherBoundsInformation()
{
  this->GetRenderer()->ComputeVisiblePropBounds(this->LastComputedBounds);
  this->SynchronizedWindows->SynchronizeBounds(this->LastComputedBounds);
}

//----------------------------------------------------------------------------
// Note this is called on all processes.
void vtkPVRenderView::ResetCamera()
{
  this->Update();
  this->GatherBoundsInformation();

  // Remember, vtkRenderer::ResetCamera() call
  // vtkRenderer::ResetCameraClippingPlanes() with the given bounds.
  this->RenderView->GetRenderer()->ResetCamera(this->LastComputedBounds);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::StillRender()
{
  vtkTimerLog::MarkStartEvent("Still Render");
  this->Render(false);
  vtkTimerLog::MarkEndEvent("Still Render");
}

//----------------------------------------------------------------------------
void vtkPVRenderView::InteractiveRender()
{
  vtkTimerLog::MarkStartEvent("Interactive Render");
  this->Render(true);
  vtkTimerLog::MarkEndEvent("Interactive Render");
}

//----------------------------------------------------------------------------
void vtkPVRenderView::Render(bool interactive)
{
  if (!interactive)
    {
    // Update all representations.
    // This should update mostly just the inputs to the representations, and maybe
    // the internal geometry filter.
    this->Update();

    // Do the vtkView::REQUEST_INFORMATION() pass.
    this->GatherRepresentationInformation();
    }

  // Gather information about geometry sizes from all representations.
  this->GatherGeometrySizeInformation();

  bool use_lod_rendering = interactive? this->GetUseLODRendering() : false;
  this->SetRequestLODRendering(use_lod_rendering);

  // Decide if we are doing remote rendering or local rendering.
  bool use_distributed_rendering = this->GetUseDistributedRendering();
  bool in_tile_display_mode = this->InTileDisplayMode();

  // When in tile-display mode, we are always doing shared rendering. However
  // when use_distributed_rendering we tell IceT that geometry is duplicated on
  // all processes.
  this->SynchronizedWindows->SetEnabled(
    use_distributed_rendering || in_tile_display_mode);
  this->SynchronizedRenderers->SetEnabled(
    use_distributed_rendering || in_tile_display_mode);
  this->SynchronizedRenderers->SetDataReplicatedOnAllProcesses(
    !use_distributed_rendering && in_tile_display_mode);

  // Build the request for REQUEST_PREPARE_FOR_RENDER().
  this->SetRequestDistributedRendering(use_distributed_rendering);

  if (in_tile_display_mode)
    {
    if (this->GetDeliverOutlineToClient())
      {
      this->RequestInformation->Remove(DELIVER_LOD_TO_CLIENT());
      this->RequestInformation->Set(DELIVER_OUTLINE_TO_CLIENT(), 1);
      }
    else
      {
      this->RequestInformation->Remove(DELIVER_OUTLINE_TO_CLIENT());
      this->RequestInformation->Set(DELIVER_LOD_TO_CLIENT(), 1);
      }
    }
  else
    {
    this->RequestInformation->Remove(DELIVER_LOD_TO_CLIENT());
    this->RequestInformation->Remove(DELIVER_OUTLINE_TO_CLIENT());
    }

  // In REQUEST_PREPARE_FOR_RENDER, this view expects all representations to
  // know the data-delivery mode.
  this->CallProcessViewRequest(
    vtkView::REQUEST_PREPARE_FOR_RENDER(),
    this->RequestInformation, this->ReplyInformationVector);

  if (use_distributed_rendering &&
    this->OrderedCompositingBSPCutsSource->GetNumberOfInputConnections(0) > 0)
    {
    this->OrderedCompositingBSPCutsSource->Update();
    this->SynchronizedRenderers->SetKdTree(
      this->OrderedCompositingBSPCutsSource->GetPKdTree());
    this->RequestInformation->Set(KD_TREE(),
      this->OrderedCompositingBSPCutsSource->GetPKdTree());
    }
  else
    {
    this->SynchronizedRenderers->SetKdTree(NULL);
    }

  this->CallProcessViewRequest(
    vtkView::REQUEST_RENDER(),
    this->RequestInformation, this->ReplyInformationVector);

  // set the image reduction factor.
  this->SynchronizedRenderers->SetImageReductionFactor(
    (interactive?
     this->InteractiveRenderImageReductionFactor :
     this->StillRenderImageReductionFactor));

  if (!interactive)
    {
    // Keep bounds information up-to-date.
    // FIXME: How can be make this so that we don't have to do parallel
    // communication each time.
    this->GatherBoundsInformation();
    }

  // Call Render() on local render window only if
  // 1: Local process is the driver OR
  // 2: RenderEventPropagation is Off and we are doing distributed rendering.
  if (this->SynchronizedWindows->GetLocalProcessIsDriver() ||
    (!this->SynchronizedWindows->GetRenderEventPropagation() &&
     use_distributed_rendering))
    {
    this->GetRenderWindow()->Render();
    }
}

//----------------------------------------------------------------------------
void vtkPVRenderView::SetRequestDistributedRendering(bool enable)
{
  bool in_tile_display_mode = this->InTileDisplayMode();
  if (enable)
    {
    this->RequestInformation->Set(DATA_DISTRIBUTION_MODE(),
      in_tile_display_mode?
      vtkMPIMoveData::COLLECT_AND_PASS_THROUGH:
      vtkMPIMoveData::PASS_THROUGH);
    }
  else
    {
    this->RequestInformation->Set(DATA_DISTRIBUTION_MODE(),
      in_tile_display_mode?
      vtkMPIMoveData::CLONE:
      vtkMPIMoveData::COLLECT);
    }
}

//----------------------------------------------------------------------------
void vtkPVRenderView::SetRequestLODRendering(bool enable)
{
  if (enable)
    {
    this->RequestInformation->Set(USE_LOD(), 1);
    this->RequestInformation->Set(LOD_RESOLUTION(), this->LODResolution);
    }
  else
    {
    this->RequestInformation->Remove(USE_LOD());
    this->RequestInformation->Remove(LOD_RESOLUTION());
    }
}

//----------------------------------------------------------------------------
void vtkPVRenderView::GatherRepresentationInformation()
{
  this->CallProcessViewRequest(vtkView::REQUEST_INFORMATION(),
    this->RequestInformation, this->ReplyInformationVector);

  // REQUEST_UPDATE() pass may result in REDISTRIBUTABLE_DATA_PRODUCER() being
  // specified. If so, we update the OrderedCompositingBSPCutsSource to use
  // those producers as inputs, if ordered compositing maybe needed.
  static vtkstd::set<void*> previous_producers;

  this->LocalGeometrySize = 0;

  bool need_ordered_compositing = false;
  vtkstd::set<void*> current_producers;
  int num_reprs = this->ReplyInformationVector->GetNumberOfInformationObjects();
  for (int cc=0; cc < num_reprs; cc++)
    {
    vtkInformation* info =
      this->ReplyInformationVector->GetInformationObject(cc);
    if (info->Has(REDISTRIBUTABLE_DATA_PRODUCER()))
      {
      current_producers.insert(info->Get(REDISTRIBUTABLE_DATA_PRODUCER()));
      }
    if (info->Has(NEED_ORDERED_COMPOSITING()))
      {
      need_ordered_compositing = true;
      }
    if (info->Has(GEOMETRY_SIZE()))
      {
      this->LocalGeometrySize += info->Get(GEOMETRY_SIZE());
      }
    }

  if (!this->GetUseOrderedCompositing() || !need_ordered_compositing)
    {
    this->OrderedCompositingBSPCutsSource->RemoveAllInputs();
    previous_producers.clear();
    return;
    }

  if (current_producers != previous_producers)
    {
    this->OrderedCompositingBSPCutsSource->RemoveAllInputs();
    vtkstd::set<void*>::iterator iter;
    for (iter = current_producers.begin(); iter != current_producers.end();
      ++iter)
      {
      this->OrderedCompositingBSPCutsSource->AddInputConnection(0,
        reinterpret_cast<vtkAlgorithm*>(*iter)->GetOutputPort(0));
      }
    previous_producers = current_producers;
    }

}

//----------------------------------------------------------------------------
void vtkPVRenderView::GatherGeometrySizeInformation()
{
  this->GeometrySize = this->LocalGeometrySize;
  this->SynchronizedWindows->SynchronizeSize(this->GeometrySize);
}

//----------------------------------------------------------------------------
bool vtkPVRenderView::GetUseDistributedRendering()
{
  return this->RemoteRenderingThreshold <= this->GeometrySize;
}

//----------------------------------------------------------------------------
bool vtkPVRenderView::GetUseLODRendering()
{
  // return false;
  return this->LODRenderingThreshold <= this->GeometrySize;
}

//----------------------------------------------------------------------------
bool vtkPVRenderView::GetDeliverOutlineToClient()
{
//  return false;
  return this->ClientOutlineThreshold <= this->GeometrySize;
}

//----------------------------------------------------------------------------
bool vtkPVRenderView::GetUseOrderedCompositing()
{
  vtkPVOptions* options = vtkProcessModule::GetProcessModule()->GetOptions();
  switch (options->GetProcessType())
    {
  case vtkPVOptions::PVSERVER:
  case vtkPVOptions::PVBATCH:
  case vtkPVOptions::PVRENDER_SERVER:
    if (vtkProcessModule::GetProcessModule()->GetNumberOfLocalPartitions() > 1)
      {
      return true;
      }
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkPVRenderView::SetUseLightKit(bool use)
{
  if (this->UseLightKit != use)
    {
    if (use)
      {
      this->LightKit->AddLightsToRenderer(this->GetRenderer());
      }
    else
      {
      this->LightKit->RemoveLightsFromRenderer(this->GetRenderer());
      }
    this->UseLightKit = use;
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkPVRenderView::SetLightSwitch(bool enable)
{
  this->Light->SetSwitch(enable? 1 : 0);
}

//----------------------------------------------------------------------------
bool vtkPVRenderView::GetLightSwitch()
{
  return this->Light->GetSwitch() != 0;
}

//----------------------------------------------------------------------------
void vtkPVRenderView::AddPropToNonCompositedRenderer(vtkProp* prop)
{
  this->GetNonCompositedRenderer()->AddActor(prop);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::RemovePropFromNonCompositedRenderer(vtkProp* prop)
{
  this->GetNonCompositedRenderer()->RemoveActor(prop);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::AddPropToRenderer(vtkProp* prop)
{
  this->GetRenderer()->AddActor(prop);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::RemovePropFromRenderer(vtkProp* prop)
{
  this->GetRenderer()->RemoveActor(prop);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "UseLightKit: " << this->UseLightKit << endl;
}

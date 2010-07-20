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

#include "vtkCommand.h"
#include "vtkInformation.h"
#include "vtkInformationIntegerKey.h"
#include "vtkInformationRequestKey.h"
#include "vtkInformationVector.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkMPIMoveData.h"
#include "vtkObjectFactory.h"
#include "vtkPVServerInformation.h"
#include "vtkPVSynchronizedRenderer.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkRemoteConnection.h"
#include "vtkRenderer.h"
#include "vtkRenderViewBase.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkWeakPointer.h"

#include "vtkActor.h"
#include "vtkProperty.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkDistributedDataFilter.h"
#include "vtkMultiProcessController.h"
#include "vtkPieceScalars.h"
#include "vtkPolyDataMapper.h"
#include "vtkSphereSource.h"
#include "vtkScalarBarActor.h"
#include "vtkLookupTable.h"
#include "vtkGeometryRepresentation.h"

#include "vtkProcessModule.h"
#include "vtkRemoteConnection.h"

#include <assert.h>

namespace
{
  static vtkWeakPointer<vtkPVSynchronizedRenderWindows> SynchronizedWindows;

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
  //-----------------------------------------------------------------------------
  void CreatePipeline(vtkMultiProcessController* controller,
    vtkRenderer* renderer, vtkView* view)
    {
    int num_procs = controller->GetNumberOfProcesses();
    int my_id = controller->GetLocalProcessId();

    vtkSphereSource* sphere = vtkSphereSource::New();
    sphere->SetPhiResolution(100);
    sphere->SetThetaResolution(100);

    vtkDistributedDataFilter* d3 = vtkDistributedDataFilter::New();
    d3->SetInputConnection(sphere->GetOutputPort());
    d3->SetController(controller);
    d3->SetBoundaryModeToSplitBoundaryCells();
    d3->UseMinimalMemoryOff();

    vtkDataSetSurfaceFilter* surface = vtkDataSetSurfaceFilter::New();
    surface->SetInputConnection(d3->GetOutputPort());

    vtkPieceScalars *piecescalars = vtkPieceScalars::New();
    piecescalars->SetInputConnection(surface->GetOutputPort());
    piecescalars->SetScalarModeToCellData();

    vtkGeometryRepresentation* repr = vtkGeometryRepresentation::New();
    repr->SetInputConnection(piecescalars->GetOutputPort());
    view->AddRepresentation(repr);
    repr->Delete();

    //vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
    //mapper->SetInputConnection(piecescalars->GetOutputPort());
    //mapper->SetScalarModeToUseCellFieldData();
    //mapper->SelectColorArray("Piece");
    //mapper->SetScalarRange(0, num_procs-1);
    //mapper->SetPiece(my_id);
    //mapper->SetNumberOfPieces(num_procs);
    //mapper->Update();

    ////this->KdTree = d3->GetKdtree();

    //vtkActor* actor = vtkActor::New();
    //actor->SetMapper(mapper);
    ////actor->GetProperty()->SetOpacity(this->UseOrderedCompositing? 0.5 : 1.0);
    ////actor->GetProperty()->SetOpacity(0.5);
    //renderer->AddActor(actor);

    //actor->Delete();
    //mapper->Delete();
    piecescalars->Delete();
    surface->Delete();
    d3->Delete();
    sphere->Delete();
    }
};

vtkStandardNewMacro(vtkPVRenderView);
vtkInformationKeyMacro(vtkPVRenderView, GEOMETRY_SIZE, Integer);
vtkInformationKeyMacro(vtkPVRenderView, DATA_DISTRIBUTION_MODE, Integer);
vtkInformationKeyMacro(vtkPVRenderView, USE_LOD, Integer);
vtkInformationKeyMacro(vtkPVRenderView, DELIVER_OUTLINE_TO_CLIENT, Integer);
vtkInformationKeyMacro(vtkPVRenderView, DELIVER_LOD_TO_CLIENT, Integer);
vtkInformationKeyMacro(vtkPVRenderView, LOD_RESOLUTION, Integer);
//----------------------------------------------------------------------------
vtkPVRenderView::vtkPVRenderView()
{
  this->StillRenderImageReductionFactor = 1;
  this->InteractiveRenderImageReductionFactor = 2;
  this->GeometrySize = 0;
  this->RemoteRenderingThreshold = 0;
  this->LODRenderingThreshold = 0;
  this->ClientOutlineThreshold = 100;

  if (::SynchronizedWindows == NULL)
    {
    this->SynchronizedWindows = vtkPVSynchronizedRenderWindows::New();
    ::SynchronizedWindows = this->SynchronizedWindows;
    }
  else
    {
    this->SynchronizedWindows = ::SynchronizedWindows;
    this->SynchronizedWindows->Register(this);
    }

  this->SynchronizedRenderers = vtkPVSynchronizedRenderer::New();

  vtkRenderWindow* window = this->SynchronizedWindows->NewRenderWindow();
  this->RenderView = vtkRenderViewBase::New();
  this->RenderView->SetRenderWindow(window);
  window->Delete();

  this->Identifier = 0;

  this->NonCompositedRenderer = vtkRenderer::New();
  this->NonCompositedRenderer->EraseOff();
  this->NonCompositedRenderer->InteractiveOff();
  this->NonCompositedRenderer->SetLayer(2);
  this->NonCompositedRenderer->SetActiveCamera(
    this->RenderView->GetRenderer()->GetActiveCamera());
  window->AddRenderer(this->NonCompositedRenderer);
  window->SetNumberOfLayers(3);

  // We don't add the LabelRenderer.

  // DUMMY SPHERE FOR TESTING
  //::CreatePipeline(vtkMultiProcessController::GetGlobalController(),
  //  this->RenderView->GetRenderer(), this);

  ::Create2DPipeline(this->NonCompositedRenderer);

  vtkMemberFunctionCommand<vtkPVRenderView>* observer =
    vtkMemberFunctionCommand<vtkPVRenderView>::New();
  observer->SetCallback(*this, &vtkPVRenderView::ResetCameraClippingRange);
  this->GetRenderer()->AddObserver(vtkCommand::ResetCameraClippingRangeEvent,
    observer);
  observer->FastDelete();
}

//----------------------------------------------------------------------------
vtkPVRenderView::~vtkPVRenderView()
{
  this->SynchronizedWindows->RemoveAllRenderers(this->Identifier);
  this->SynchronizedWindows->RemoveRenderWindow(this->Identifier);
  this->SynchronizedWindows->Delete();
  this->SynchronizedRenderers->Delete();
  this->NonCompositedRenderer->Delete();
  this->RenderView->Delete();
}

//----------------------------------------------------------------------------
void vtkPVRenderView::Initialize(unsigned int id)
{
  assert(this->Identifier == 0 && id != 0);

  this->Identifier = id;
  this->SynchronizedWindows->AddRenderWindow(id, this->RenderView->GetRenderWindow());
  this->SynchronizedWindows->AddRenderer(id, this->RenderView->GetRenderer());
  this->SynchronizedWindows->AddRenderer(id, this->GetNonCompositedRenderer());
  this->SynchronizedRenderers->SetRenderer(this->RenderView->GetRenderer());
}

//----------------------------------------------------------------------------
void vtkPVRenderView::SetPosition(int x, int y)
{
  assert(this->Identifier != 0);
  this->SynchronizedWindows->SetWindowPosition(this->Identifier, x, y);
}

//----------------------------------------------------------------------------
void vtkPVRenderView::SetSize(int x, int y)
{
  assert(this->Identifier != 0);
  this->SynchronizedWindows->SetWindowSize(this->Identifier, x, y);
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
// Note this is called on all processes.
void vtkPVRenderView::ResetCamera()
{
  this->Update();

  this->SynchronizedRenderers->ComputeVisiblePropBounds(this->LastComputedBounds);

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
  // Update all representations.
  // This should update mostly just the inputs to the representations, and maybe
  // the internal geometry filter.
  this->Update();

  // Gather information about geometry sizes from all representations.
  this->GatherGeometrySizeInformation();

  bool use_lod_rendering = interactive? this->GetUseLODRendering() : false;
  this->SetRequestLODRendering(use_lod_rendering);

  // Decide if we are doing remote rendering or local rendering.
  bool use_distributed_rendering = this->GetUseDistributedRendering();
  this->SynchronizedWindows->SetEnabled(use_distributed_rendering);
  this->SynchronizedRenderers->SetEnabled(use_distributed_rendering);

  // Build the request for REQUEST_PREPARE_FOR_RENDER().
  this->SetRequestDistributedRendering(use_distributed_rendering);

  // TODO: Add more info about ordered compositing/tile-display etc.

  if (this->InTileDisplayMode())
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

  this->CallProcessViewRequest(
    vtkView::REQUEST_PREPARE_FOR_RENDER(),
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
    this->SynchronizedRenderers->ComputeVisiblePropBounds(this->LastComputedBounds);
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
    }
  else
    {
    this->RequestInformation->Remove(USE_LOD());
    }
}

//----------------------------------------------------------------------------
void vtkPVRenderView::GatherGeometrySizeInformation()
{
  this->GeometrySize = 0;

  this->CallProcessViewRequest(vtkView::REQUEST_INFORMATION(),
    this->RequestInformation, this->ReplyInformationVector);

  unsigned long geometry_size = 0;
  int num_reprs = this->ReplyInformationVector->GetNumberOfInformationObjects();
  for (int cc=0; cc < num_reprs; cc++)
    {
    vtkInformation* info =
      this->ReplyInformationVector->GetInformationObject(cc);
    if (info->Has(GEOMETRY_SIZE()))
      {
      geometry_size += info->Get(GEOMETRY_SIZE());
      }
    }

  // Now synchronize the geometry size.
  vtkMultiProcessController* parallelController =
    this->SynchronizedWindows->GetParallelController();
  vtkMultiProcessController* csController =
    this->SynchronizedWindows->GetClientServerController();

  if (parallelController)
    {
    unsigned long reduced_value=0;
    parallelController->AllReduce(&geometry_size, &reduced_value, 1,
      vtkCommunicator::SUM_OP);
    geometry_size = reduced_value;
    }

  if (csController)
    {
    // FIXME: won't it be easier to add some support for collective operations
    // to the vtkSocketCommunicator?
    if (this->SynchronizedWindows->GetLocalProcessIsDriver())
      {
      // client
      unsigned long value;
      csController->Broadcast(&geometry_size, 1, 0);
      csController->Broadcast(&value, 1, 1);
      geometry_size += value;
      }
    else
      {
      // server-root.
      unsigned long value;
      csController->Broadcast(&value, 1, 1);
      csController->Broadcast(&geometry_size, 1, 0);
      geometry_size += value;
      }
    }

  if (parallelController)
    {
    unsigned long reduced_value=0;
    parallelController->AllReduce(&geometry_size, &reduced_value, 1,
      vtkCommunicator::SUM_OP);
    geometry_size = reduced_value;
    }

  this->GeometrySize = geometry_size;
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
bool vtkPVRenderView::InTileDisplayMode()
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
void vtkPVRenderView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

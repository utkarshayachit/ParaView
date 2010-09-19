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
#include "vtkGeometryRepresentation.h"

#include "vtkCommand.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkOrderedCompositeDistributor.h"
#include "vtkPKdTree.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkPVGeometryFilter.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkQuadricClustering.h"
#include "vtkRenderer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredDataDeliveryFilter.h"

vtkStandardNewMacro(vtkGeometryRepresentation);
//----------------------------------------------------------------------------
vtkGeometryRepresentation::vtkGeometryRepresentation()
{
  this->GeometryFilter = vtkPVGeometryFilter::New();
  this->Decimator = vtkQuadricClustering::New();
  this->Decimator->SetNumberOfDivisions(10, 10, 10);
  this->Mapper = vtkPolyDataMapper::New();
  this->LODMapper = vtkPolyDataMapper::New();
  this->Actor = vtkPVLODActor::New();
  this->Property = vtkProperty::New();
  //this->Property->SetOpacity(0.5);
  this->DeliveryFilter = vtkUnstructuredDataDeliveryFilter::New();
  this->LODDeliveryFilter = vtkUnstructuredDataDeliveryFilter::New();

  this->GeometryFilter->SetUseOutline(0);

  this->DeliveryFilter->SetOutputDataType(VTK_POLY_DATA);
  this->LODDeliveryFilter->SetOutputDataType(VTK_POLY_DATA);

  this->Distributor = vtkOrderedCompositeDistributor::New();
  this->Distributor->SetController(vtkMultiProcessController::GetGlobalController());
  this->Distributor->SetInputConnection(0, this->DeliveryFilter->GetOutputPort());

  this->Decimator->SetInputConnection(this->GeometryFilter->GetOutputPort());
  this->Mapper->SetInputConnection(this->Distributor->GetOutputPort());
  this->LODMapper->SetInputConnection(this->LODDeliveryFilter->GetOutputPort());
  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetLODMapper(this->LODMapper);
  this->Actor->SetProperty(this->Property);
}

//----------------------------------------------------------------------------
vtkGeometryRepresentation::~vtkGeometryRepresentation()
{
  this->GeometryFilter->Delete();
  this->Decimator->Delete();
  this->Mapper->Delete();
  this->LODMapper->Delete();
  this->Actor->Delete();
  this->Property->Delete();
  this->DeliveryFilter->Delete();
  this->LODDeliveryFilter->Delete();
  this->Distributor->Delete();
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetColor(double r, double g, double b)
{
  this->Property->SetColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetLineWidth(double val)
{
  this->Property->SetLineWidth(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOpacity(double val)
{
  this->Property->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPointSize(double val)
{
  this->Property->SetPointSize(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetRepresentation(int val)
{
  this->Property->SetRepresentation(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetVisibility(int val)
{
  this->Actor->SetVisibility(val);
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (request_type == vtkView::REQUEST_INFORMATION())
    {
    this->GenerateMetaData(inInfo, outInfo);
    }
  else if (request_type == vtkView::REQUEST_PREPARE_FOR_RENDER())
    {
    // In REQUEST_PREPARE_FOR_RENDER, we need to ensure all our data-deliver
    // filters have their states updated as requested by the view.

    // this is where we will look to see on what nodes are we going to render and
    // render set that up.
    this->DeliveryFilter->ProcessViewRequest(inInfo);
    this->LODDeliveryFilter->ProcessViewRequest(inInfo);
    bool lod = inInfo->Has(vtkPVRenderView::USE_LOD());
    if (lod)
      {
      if (inInfo->Has(vtkPVRenderView::LOD_RESOLUTION()))
        {
        int division = static_cast<int>(150 *
          inInfo->Get(vtkPVRenderView::LOD_RESOLUTION())) + 10;
        this->Decimator->SetNumberOfDivisions(division, division, division);
        }
      this->LODDeliveryFilter->Update();
      }
    else
      {
      this->DeliveryFilter->Update();
      }
    this->Actor->SetEnableLOD(lod? 1 : 0);
    }
  else if (request_type == vtkView::REQUEST_RENDER())
    {
    // typically, representations don't do anything special in this pass.
    // However, when we are doing ordered compositing, we need to ensure that
    // the redistribution of data happens in this pass.
    if (inInfo->Has(vtkPVRenderView::KD_TREE()))
      {
      vtkPKdTree* kdTree = vtkPKdTree::SafeDownCast(
        inInfo->Get(vtkPVRenderView::KD_TREE()));
      this->Distributor->SetPKdTree(kdTree);
      this->Distributor->SetPassThrough(0);
      }
    else
      {
      this->Distributor->SetPKdTree(NULL);
      this->Distributor->SetPassThrough(1);
      }
    this->Actor->GetMapper()->Update();
    }

  return this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestData(vtkInformation*,
  vtkInformationVector** inputVector, vtkInformationVector*)
{
  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    this->GeometryFilter->SetInputConnection(
      this->GetInternalOutputPort());
    cout << "Update GeometryFilter" << endl;
    this->GeometryFilter->Update();
    this->DeliveryFilter->SetInputConnection(
      this->GeometryFilter->GetOutputPort());
    this->LODDeliveryFilter->SetInputConnection(
      this->Decimator->GetOutputPort());
    }
  else
    {
    cout << "Process has not input data" << endl;
    this->DeliveryFilter->RemoveAllInputs();
    this->LODDeliveryFilter->RemoveAllInputs();
    }

  // We fire UpdateDataEvent to notify the representation proxy that the
  // representation was updated. The representation proxty will then call
  // PostUpdateData(). We do this since now representations are not updated at
  // the proxy level.
  this->InvokeEvent(vtkCommand::UpdateDataEvent);
  return 1;
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{

  this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);

  // ideally, extent and time information will come from the view in
  // REQUEST_UPDATE(), include view-time.
  vtkMultiProcessController* controller =
    vtkMultiProcessController::GetGlobalController();
  if (controller && inputVector[0]->GetNumberOfInformationObjects() == 1)
    {
    vtkStreamingDemandDrivenPipeline* sddp =
      vtkStreamingDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
    sddp->SetUpdateExtent(inputVector[0]->GetInformationObject(0),
      controller->GetLocalProcessId(),
      controller->GetNumberOfProcesses(), 0);
    }
  return 1;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::GenerateMetaData(vtkInformation*,
  vtkInformation* outInfo)
{
  vtkDataObject* geom = this->GeometryFilter->GetOutputDataObject(0);
  if (geom)
    {
    outInfo->Set(vtkPVRenderView::GEOMETRY_SIZE(),geom->GetActualMemorySize());
    }

  outInfo->Set(vtkPVRenderView::REDISTRIBUTABLE_DATA_PRODUCER(),
    this->DeliveryFilter);
  if (this->Actor->GetProperty()->GetOpacity() < 1.0)
    {
    outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);
    }
  return 1;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::MarkModified()
{
  this->GeometryFilter->Modified();
  this->DeliveryFilter->Modified();
  this->LODDeliveryFilter->Modified();
  this->Distributor->Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::AddToView(vtkView* view)
{
  // FIXME: Need generic view API to add props.
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->AddActor(this->Actor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->RemoveActor(this->Actor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


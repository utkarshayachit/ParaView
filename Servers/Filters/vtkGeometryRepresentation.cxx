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

#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
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
  this->Mapper = vtkPolyDataMapper::New();
  this->LODMapper = vtkPolyDataMapper::New();
  this->Actor = vtkPVLODActor::New();
  this->Property = vtkProperty::New();
  this->DeliverFilter = vtkUnstructuredDataDeliveryFilter::New();

  this->GeometryFilter->SetUseOutline(0);

  this->DeliverFilter->SetOutputDataType(VTK_POLY_DATA);

  this->Decimator->SetInputConnection(this->GeometryFilter->GetOutputPort());
  this->Mapper->SetInputConnection(this->DeliverFilter->GetOutputPort());
  this->LODMapper->SetInputConnection(this->Decimator->GetOutputPort());
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
  this->DeliverFilter->Delete();
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (request_type == vtkView::REQUEST_INFORMATION())
    {
    this->RequestMetaData(inInfo, outInfo);
    }
  else if (request_type == vtkView::REQUEST_PREPARE_FOR_RENDER())
    {
    this->PrepareForRendering(inInfo, outInfo);
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
    this->GeometryFilter->Update();
    this->DeliverFilter->SetInputConnection(
      this->GeometryFilter->GetOutputPort());
    }
  else
    {
    this->DeliverFilter->RemoveAllInputs();
    }
  return 1;
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
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

  return this->Superclass::RequestUpdateExtent(request, inputVector,
    outputVector);
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::RequestMetaData(vtkInformation*,
  vtkInformation* outInfo)
{
  vtkDataObject* geom = this->GeometryFilter->GetOutputDataObject(0);
  if (geom)
    {
    outInfo->Set(vtkPVRenderView::GEOMETRY_SIZE(),geom->GetActualMemorySize());
    }
  return 1; 
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::PrepareForRendering(
  vtkInformation* inInfo, vtkInformation*)
{
  // this is where we will look to see on what nodes are we going to render and
  // render set that up.
  this->DeliverFilter->ProcessViewRequest(inInfo);
  this->DeliverFilter->Update();
  return true;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::AddToView(vtkView* view)
{
  // FIXME: Need generic view API to add props.
  vtkPVRenderView::SafeDownCast(view)->GetRenderer()->AddActor(this->Actor);
  return true;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->RemoveActor(this->Actor);
    }
  return true;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


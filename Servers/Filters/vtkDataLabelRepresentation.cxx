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
#include "vtkDataLabelRepresentation.h"

#include "vtkActor2D.h"
#include "vtkCellCenters.h"
#include "vtkCommand.h"
#include "vtkCompositeDataToUnstructuredGridFilter.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkLabeledDataMapper.h"
#include "vtkMPIMoveData.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVRenderView.h"
#include "vtkRenderer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTextProperty.h"
#include "vtkUnstructuredDataDeliveryFilter.h"

vtkStandardNewMacro(vtkDataLabelRepresentation);
//----------------------------------------------------------------------------
vtkDataLabelRepresentation::vtkDataLabelRepresentation()
{
  this->MergeBlocks = vtkCompositeDataToUnstructuredGridFilter::New();
  this->DataCollector = vtkUnstructuredDataDeliveryFilter::New();

  this->PointLabelMapper = vtkLabeledDataMapper::New();
  this->PointLabelActor = vtkActor2D::New();
  this->PointLabelProperty = vtkTextProperty::New();

  this->CellCenters = vtkCellCenters::New();
  this->CellLabelMapper = vtkLabeledDataMapper::New();
  this->CellLabelActor = vtkActor2D::New();
  this->CellLabelProperty = vtkTextProperty::New();

  this->DataCollector->SetOutputDataType(VTK_UNSTRUCTURED_GRID);

  this->PointLabelMapper->SetInputConnection(this->DataCollector->GetOutputPort());
  this->CellCenters->SetInputConnection(this->DataCollector->GetOutputPort());
  this->CellLabelMapper->SetInputConnection(this->CellCenters->GetOutputPort());

  this->PointLabelActor->SetMapper(this->PointLabelMapper);
  this->CellLabelActor->SetMapper(this->CellLabelMapper);

  this->PointLabelMapper->SetLabelTextProperty(this->PointLabelProperty);
  this->CellLabelMapper->SetLabelTextProperty(this->CellLabelProperty);

  this->InitializeForCommunication();
}

//----------------------------------------------------------------------------
vtkDataLabelRepresentation::~vtkDataLabelRepresentation()
{
  this->MergeBlocks->Delete();
  this->DataCollector->Delete();
  this->PointLabelMapper->Delete();
  this->PointLabelActor->Delete();
  this->PointLabelProperty->Delete();
  this->CellCenters->Delete();
  this->CellLabelMapper->Delete();
  this->CellLabelActor->Delete();
  this->CellLabelProperty->Delete();
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::InitializeForCommunication()
{
  vtkInformation* info = vtkInformation::New();
  info->Set(vtkPVRenderView::DATA_DISTRIBUTION_MODE(),
    vtkMPIMoveData::CLONE);
  this->DataCollector->ProcessViewRequest(info);
  info->Delete();
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelVisibility(int val)
{
  this->PointLabelActor->SetVisibility(val);
}
//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointFieldDataArrayName(const char* val)
{
  this->PointLabelMapper->SetFieldDataName(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelMode(int val)
{
  this->PointLabelMapper->SetLabelMode(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelColor(double r, double g, double b)
{
  this->PointLabelProperty->SetColor(r, g, b);
}
//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelOpacity(double val)
{
  this->PointLabelProperty->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelFontFamily(int val)
{
  this->PointLabelProperty->SetFontFamily(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelBold(int val)
{
  this->PointLabelProperty->SetBold(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelItalic(int val)
{
  this->PointLabelProperty->SetItalic(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelShadow(int val)
{
  this->PointLabelProperty->SetShadow(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelJustification(int val)
{
  this->PointLabelProperty->SetJustification(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetPointLabelFontSize(int val)
{
  this->PointLabelProperty->SetFontSize(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelVisibility(int val)
{
  this->CellLabelActor->SetVisibility(val);
}
//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellFieldDataArrayName(const char* val)
{
  this->CellLabelMapper->SetFieldDataName(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelMode(int val)
{
  this->CellLabelMapper->SetLabelMode(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelColor(double r, double g, double b)
{
  this->CellLabelProperty->SetColor(r, g, b);
}
//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelOpacity(double val)
{
  this->CellLabelProperty->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelFontFamily(int val)
{
  this->CellLabelProperty->SetFontFamily(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelBold(int val)
{
  this->CellLabelProperty->SetBold(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelItalic(int val)
{
  this->CellLabelProperty->SetItalic(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelShadow(int val)
{
  this->CellLabelProperty->SetShadow(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelJustification(int val)
{
  this->CellLabelProperty->SetJustification(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::SetCellLabelFontSize(int val)
{
  this->CellLabelProperty->SetFontSize(val);
}

//----------------------------------------------------------------------------
void vtkDataLabelRepresentation::MarkModified()
{
  this->DataCollector->Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkDataLabelRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
bool vtkDataLabelRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetNonCompositedRenderer()->AddActor(this->PointLabelActor);
    rview->GetNonCompositedRenderer()->AddActor(this->CellLabelActor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkDataLabelRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetNonCompositedRenderer()->RemoveActor(this->PointLabelActor);
    rview->GetNonCompositedRenderer()->RemoveActor(this->CellLabelActor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
int vtkDataLabelRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (request_type == vtkView::REQUEST_PREPARE_FOR_RENDER())
    {
    // In REQUEST_PREPARE_FOR_RENDER, we need to ensure all our data-deliver
    // filters have their states updated as requested by the view.
    this->DataCollector->Update();
    }

  return this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
}

//----------------------------------------------------------------------------
int vtkDataLabelRepresentation::RequestData(vtkInformation*,
  vtkInformationVector** inputVector, vtkInformationVector*)
{
  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    this->MergeBlocks->SetInputConnection(
      this->GetInternalOutputPort());
    this->MergeBlocks->Update();
    this->DataCollector->SetInputConnection(this->MergeBlocks->GetOutputPort());
    }
  else
    {
    this->MergeBlocks->RemoveAllInputs();
    this->DataCollector->RemoveAllInputs();
    }

  // We fire UpdateDataEvent to notify the representation proxy that the
  // representation was updated. The representation proxty will then call
  // PostUpdateData(). We do this since now representations are not updated at
  // the proxy level.
  this->InvokeEvent(vtkCommand::UpdateDataEvent);
  return 1;
}

//----------------------------------------------------------------------------
int vtkDataLabelRepresentation::RequestUpdateExtent(vtkInformation* request,
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
void vtkDataLabelRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

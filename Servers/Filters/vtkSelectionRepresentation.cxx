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
#include "vtkSelectionRepresentation.h"

#include "vtkDataLabelRepresentation.h"
#include "vtkGeometryRepresentation.h"
#include "vtkInformation.h"
#include "vtkLabeledDataMapper.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkObjectFactory.h"
#include "vtkView.h"

vtkStandardNewMacro(vtkSelectionRepresentation);
vtkCxxSetObjectMacro(vtkSelectionRepresentation, LabelRepresentation,
  vtkDataLabelRepresentation);
//----------------------------------------------------------------------------
vtkSelectionRepresentation::vtkSelectionRepresentation()
{
  this->GeometryRepresentation = vtkGeometryRepresentation::New();
  this->LabelRepresentation = vtkDataLabelRepresentation::New();
  this->LabelRepresentation->SetPointLabelMode(VTK_LABEL_FIELD_DATA);
  this->LabelRepresentation->SetCellLabelMode(VTK_LABEL_FIELD_DATA);

  vtkCommand* observer = vtkMakeMemberFunctionCommand(*this,
    &vtkSelectionRepresentation::TriggerUpdateDataEvent);
  this->GeometryRepresentation->AddObserver(vtkCommand::UpdateDataEvent,
    observer);
  this->LabelRepresentation->AddObserver(vtkCommand::UpdateDataEvent,
    observer);
  observer->Delete();
}

//----------------------------------------------------------------------------
vtkSelectionRepresentation::~vtkSelectionRepresentation()
{
  this->GeometryRepresentation->Delete();
  this->LabelRepresentation->Delete();
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetInputConnection(
  int port, vtkAlgorithmOutput* input)
{
  this->GeometryRepresentation->SetInputConnection(port, input);
  this->LabelRepresentation->SetInputConnection(port, input);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetInputConnection(vtkAlgorithmOutput* input)
{
  this->GeometryRepresentation->SetInputConnection(input);
  this->LabelRepresentation->SetInputConnection(input);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::AddInputConnection(
  int port, vtkAlgorithmOutput* input)
{
  this->GeometryRepresentation->AddInputConnection(port, input);
  this->LabelRepresentation->AddInputConnection(port, input);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::AddInputConnection(vtkAlgorithmOutput* input)
{
  this->GeometryRepresentation->AddInputConnection(input);
  this->LabelRepresentation->AddInputConnection(input);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::RemoveInputConnection(
  int port, vtkAlgorithmOutput* input)
{
  this->GeometryRepresentation->RemoveInputConnection(port, input);
  this->LabelRepresentation->RemoveInputConnection(port, input);
}

//----------------------------------------------------------------------------
int vtkSelectionRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
bool vtkSelectionRepresentation::AddToView(vtkView* view)
{
  view->AddRepresentation(this->GeometryRepresentation);
  view->AddRepresentation(this->LabelRepresentation);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSelectionRepresentation::RemoveFromView(vtkView* view)
{
  view->RemoveRepresentation(this->GeometryRepresentation);
  view->RemoveRepresentation(this->LabelRepresentation);
  return true;
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::MarkModified()
{
  this->GeometryRepresentation->MarkModified();
  this->LabelRepresentation->MarkModified();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::TriggerUpdateDataEvent()
{
  // We fire UpdateDataEvent to notify the representation proxy that the
  // representation was updated. The representation proxty will then call
  // PostUpdateData(). We do this since now representations are not updated at
  // the proxy level.
  this->InvokeEvent(vtkCommand::UpdateDataEvent);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetColor(double r, double g, double b)
{
  this->GeometryRepresentation->SetColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetLineWidth(double val)
{
  this->GeometryRepresentation->SetLineWidth(val);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetOpacity(double val)
{
  this->GeometryRepresentation->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetPointSize(double val)
{
  this->GeometryRepresentation->SetPointSize(val);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetVisibility(int val)
{
  this->GeometryRepresentation->SetVisibility(val);
}

//----------------------------------------------------------------------------
void vtkSelectionRepresentation::SetRepresentation(int val)
{
  this->GeometryRepresentation->SetRepresentation(val);
}

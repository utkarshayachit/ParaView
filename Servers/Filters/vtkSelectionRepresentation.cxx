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
#include "vtkObjectFactory.h"
#include "vtkView.h"

vtkStandardNewMacro(vtkSelectionRepresentation);
vtkCxxSetObjectMacro(vtkSelectionRepresentation, GeometryRepresentation,
  vtkGeometryRepresentation);
vtkCxxSetObjectMacro(vtkSelectionRepresentation, LabelRepresentation,
  vtkDataLabelRepresentation);
//----------------------------------------------------------------------------
vtkSelectionRepresentation::vtkSelectionRepresentation()
{
  this->GeometryRepresentation = vtkGeometryRepresentation::New();
  this->LabelRepresentation = vtkDataLabelRepresentation::New();
  this->LabelRepresentation->SetPointLabelMode(VTK_LABEL_FIELD_DATA);
  this->LabelRepresentation->SetCellLabelMode(VTK_LABEL_FIELD_DATA);
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
void vtkSelectionRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

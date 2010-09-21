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
#include "vtkPVDataRepresentation.h"

#include "vtkCubeAxesRepresentation.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkSelectionRepresentation.h"
#include "vtkView.h"

vtkStandardNewMacro(vtkPVDataRepresentation);
//----------------------------------------------------------------------------
vtkPVDataRepresentation::vtkPVDataRepresentation()
{
  this->SetNumberOfInputPorts(2);
  this->SelectionRepresentation = vtkSelectionRepresentation::New();
  this->CubeAxesRepresentation = vtkCubeAxesRepresentation::New();
}

//----------------------------------------------------------------------------
vtkPVDataRepresentation::~vtkPVDataRepresentation()
{
  this->SelectionRepresentation->Delete();
  this->CubeAxesRepresentation->Delete();
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::SetInputConnection(int port, vtkAlgorithmOutput* input)
{
  if (port == 0)
    {
    this->CubeAxesRepresentation->SetInputConnection(0, input);
    this->Superclass::SetInputConnection(0, input);
    }
  else if (port == 1)
    {
    this->SelectionRepresentation->SetInputConnection(0, input);
    }
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::SetInputConnection(vtkAlgorithmOutput* input)
{
  // port is assumed to be 0.
  this->CubeAxesRepresentation->SetInputConnection(input);
  this->Superclass::SetInputConnection(input);
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::AddInputConnection(
  int port, vtkAlgorithmOutput* input)
{
 if (port == 0)
    {
    this->CubeAxesRepresentation->AddInputConnection(0, input);
    this->Superclass::AddInputConnection(0, input);
    }
  else if (port == 1)
    {
    this->SelectionRepresentation->AddInputConnection(0, input);
    }
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::AddInputConnection(vtkAlgorithmOutput* input)
{
  // port is assumed to be 0.
  this->CubeAxesRepresentation->AddInputConnection(input);
  this->Superclass::AddInputConnection(input);
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::RemoveInputConnection(int port, vtkAlgorithmOutput* input)
{
  if (port == 0)
    {
    this->CubeAxesRepresentation->RemoveInputConnection(0, input);
    this->Superclass::RemoveInputConnection(0, input);
    }
  else if (port == 1)
    {
    this->SelectionRepresentation->RemoveInputConnection(0, input);
    }
}

//----------------------------------------------------------------------------
bool vtkPVDataRepresentation::AddToView(vtkView* view)
{
  view->AddRepresentation(this->CubeAxesRepresentation);
  view->AddRepresentation(this->SelectionRepresentation);
  return this->Superclass::AddToView(view);
}

//----------------------------------------------------------------------------
bool vtkPVDataRepresentation::RemoveFromView(vtkView* view)
{
  view->RemoveRepresentation(this->CubeAxesRepresentation);
  view->RemoveRepresentation(this->SelectionRepresentation);
  return this->Superclass::RemoveFromView(view);
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::MarkModified()
{
  this->CubeAxesRepresentation->MarkModified();
  this->SelectionRepresentation->MarkModified();
  this->Superclass::MarkModified();
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

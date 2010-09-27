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
#include "vtkOutlineRepresentation.h"

#include "vtkObjectFactory.h"
#include "vtkPVGeometryFilter.h"

vtkStandardNewMacro(vtkOutlineRepresentation);
//----------------------------------------------------------------------------
vtkOutlineRepresentation::vtkOutlineRepresentation()
{
  this->GeometryFilter->SetUseOutline(1);
  // this doesn't work right now, but once it starts working it will come handy.
  this->SetSuppressLOD(1);

  this->SetAmbient(1);
  this->SetDiffuse(0);
  this->SetSpecular(0);
}

//----------------------------------------------------------------------------
vtkOutlineRepresentation::~vtkOutlineRepresentation()
{
}

//----------------------------------------------------------------------------
void vtkOutlineRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
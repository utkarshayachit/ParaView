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
#include "vtkPVCenterAxesActor.h"

#include "vtkObjectFactory.h"
#include "vtkAxes.h"
#include "vtkPolyDataMapper.h"

vtkStandardNewMacro(vtkPVCenterAxesActor);
//----------------------------------------------------------------------------
vtkPVCenterAxesActor::vtkPVCenterAxesActor()
{
  this->Axes = vtkAxes::New();
  this->Mapper = vtkPolyDataMapper::New();
  this->Mapper->SetInputConnection(this->Axes->GetOutputPort());
  this->SetMapper(this->Mapper);
}

//----------------------------------------------------------------------------
vtkPVCenterAxesActor::~vtkPVCenterAxesActor()
{
  this->Axes->Delete();
  this->Mapper->Delete();
}

//----------------------------------------------------------------------------
void vtkPVCenterAxesActor::SetSymmetric(int val)
{
  this->Axes->SetSymmetric(val);
}

//----------------------------------------------------------------------------
void vtkPVCenterAxesActor::SetComputeNormals(int val)
{
  this->Axes->SetComputeNormals(val);
}

//----------------------------------------------------------------------------
void vtkPVCenterAxesActor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

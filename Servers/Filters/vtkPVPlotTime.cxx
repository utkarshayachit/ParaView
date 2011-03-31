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
#include "vtkPVPlotTime.h"

#include "vtkContext2D.h"
#include "vtkObjectFactory.h"
#include "vtkPen.h"

vtkStandardNewMacro(vtkPVPlotTime);
//----------------------------------------------------------------------------
vtkPVPlotTime::vtkPVPlotTime()
{
  // paraview green.
  this->GetPen()->SetColor(143,216,109);
}

//----------------------------------------------------------------------------
vtkPVPlotTime::~vtkPVPlotTime()
{
}

//-----------------------------------------------------------------------------
bool vtkPVPlotTime::Paint(vtkContext2D *painter)
{
  painter->ApplyPen(this->GetPen());
  painter->DrawLine(0, VTK_FLOAT_MIN, 0, VTK_FLOAT_MAX);
  return true;
}

//----------------------------------------------------------------------------
void vtkPVPlotTime::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

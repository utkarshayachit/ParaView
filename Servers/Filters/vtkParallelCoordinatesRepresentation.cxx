/*=========================================================================

  Program:   ParaView
  Module:    vtkParallelCoordinatesRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkParallelCoordinatesRepresentation.h"

#include "vtkChartParallelCoordinates.h"
#include "vtkObjectFactory.h"
#include "vtkPen.h"
#include "vtkPlot.h"
#include "vtkPVContextView.h"
#include "vtkTable.h"

vtkStandardNewMacro(vtkParallelCoordinatesRepresentation);
//----------------------------------------------------------------------------
vtkParallelCoordinatesRepresentation::vtkParallelCoordinatesRepresentation()
{
}

//----------------------------------------------------------------------------
vtkParallelCoordinatesRepresentation::~vtkParallelCoordinatesRepresentation()
{
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkChartParallelCoordinates* vtkParallelCoordinatesRepresentation::GetChart()
{
  if (this->ContextView)
    {
    return vtkChartParallelCoordinates::SafeDownCast(
      this->ContextView->GetChart());
    }

  return 0;
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetVisibility(int visible)
{
  this->Superclass::SetVisibility(visible);
  if (this->GetChart())
    {
    this->GetChart()->SetVisible(visible != 0);
    }
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetSeriesVisibility(
    const char* name, int visible)
{
  if (this->GetChart())
    {
    this->GetChart()->SetColumnVisibility(name, visible != 0);
    }
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetLabel(const char*,
                                                           const char*)
{

}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetLineThickness(int value)
{
  if (this->GetChart())
    {
    this->GetChart()->GetPlot(0)->GetPen()->SetWidth(value);
    }
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetLineStyle(int value)
{
  if (this->GetChart())
    {
    this->GetChart()->GetPlot(0)->GetPen()->SetLineType(value);
    }
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetColor(double r, double g,
                                                           double b)
{
  if (this->GetChart())
    {
    this->GetChart()->GetPlot(0)->GetPen()->SetColorF(r, g, b);
    }
}

//----------------------------------------------------------------------------
void vtkParallelCoordinatesRepresentation::SetOpacity(double opacity)
{
  if (this->GetChart())
    {
    this->GetChart()->GetPlot(0)->GetPen()->SetOpacityF(opacity);
    }
}

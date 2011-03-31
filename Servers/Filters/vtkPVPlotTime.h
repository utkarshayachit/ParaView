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
// .NAME vtkPVPlotTime - takes care of drawing a "time" marker in the plot.
// .SECTION Description
// vtkPVPlotTime is used to add a "current-time" marker to the plot when on of
// the axes in the plots is time. Currently only X-axis as time is supported.

#ifndef __vtkPVPlotTime_h
#define __vtkPVPlotTime_h

#include "vtkPlot.h"

class VTK_EXPORT vtkPVPlotTime : public vtkPlot
{
public:
  static vtkPVPlotTime* New();
  vtkTypeMacro(vtkPVPlotTime, vtkPlot);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Paint event for the axis, called whenever the axis needs to be drawn
  virtual bool Paint(vtkContext2D *painter);

//BTX
protected:
  vtkPVPlotTime();
  ~vtkPVPlotTime();

private:
  vtkPVPlotTime(const vtkPVPlotTime&); // Not implemented
  void operator=(const vtkPVPlotTime&); // Not implemented
//ETX
};

#endif

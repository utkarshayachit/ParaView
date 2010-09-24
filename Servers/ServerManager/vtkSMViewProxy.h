/*=========================================================================

  Program:   ParaView
  Module:    vtkSMViewProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMViewProxy - Superclass for all view proxies.
// .SECTION Description
// vtkSMViewProxy is a superclass for all view proxies. A view proxy
// abstracts the logic to take one or more representation proxies and show then
// in some viewport such as vtkRenderWindow.
// This class may directly be used as the view proxy for views that do all the
// rendering work at the GUI level. The VTKObject corresponding to this class
// has to be a vtkView subclass.
// .SECTION Events
// \li vtkCommand::StartEvent(callData: int:0) -- start of StillRender().
// \li vtkCommand::EndEvent(callData: int:0) -- end of StillRender().
// \li vtkCommand::StartEvent(callData: int:1) -- start of InteractiveRender().
// \li vtkCommand::EndEvent(callData: int:1) -- end of InteractiveRender().

#ifndef __vtkSMViewProxy_h
#define __vtkSMViewProxy_h

#include "vtkSMProxy.h"

class vtkImageData;

class VTK_EXPORT vtkSMViewProxy : public vtkSMProxy
{
public:
  static vtkSMViewProxy* New();
  vtkTypeMacro(vtkSMViewProxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Renders the view using full resolution.
  virtual void StillRender();

  // Description:
  // Renders the view using lower resolution is possible.
  virtual void InteractiveRender();

  // Description:
  // Captures a image from this view. Default implementation returns NULL.
  virtual vtkImageData* CaptureWindow(int vtkNotUsed(magnification))
    { return NULL; }

//BTX
protected:
  vtkSMViewProxy();
  ~vtkSMViewProxy();

private:
  vtkSMViewProxy(const vtkSMViewProxy&); // Not implemented
  void operator=(const vtkSMViewProxy&); // Not implemented

//ETX
};

#endif


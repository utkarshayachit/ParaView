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
// .NAME vtkSMRenderView2Proxy
// .SECTION Description
//

#ifndef __vtkSMRenderView2Proxy_h
#define __vtkSMRenderView2Proxy_h

#include "vtkSMProxy.h"

class VTK_EXPORT vtkSMRenderView2Proxy : public vtkSMProxy
{
public:
  static vtkSMRenderView2Proxy* New();
  vtkTypeRevisionMacro(vtkSMRenderView2Proxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

//BTX
protected:
  vtkSMRenderView2Proxy();
  ~vtkSMRenderView2Proxy();

  void OnSelect(vtkObject*, unsigned long, void* vregion);
  // Description:
  // Called at the end of CreateVTKObjects().
  virtual void CreateVTKObjects();

private:
  vtkSMRenderView2Proxy(const vtkSMRenderView2Proxy&); // Not implemented
  void operator=(const vtkSMRenderView2Proxy&); // Not implemented
//ETX
};

#endif

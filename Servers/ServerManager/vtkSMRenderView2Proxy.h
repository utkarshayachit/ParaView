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

#include "vtkSMViewProxy.h"

class vtkCollection;

class VTK_EXPORT vtkSMRenderView2Proxy : public vtkSMViewProxy
{
public:
  static vtkSMRenderView2Proxy* New();
  vtkTypeRevisionMacro(vtkSMRenderView2Proxy, vtkSMViewProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Makes a new selection source proxy.
  bool SelectSurfaceCells(int region[4],
    vtkCollection* selectedRepresentations,
    vtkCollection* selectionSources,
    bool multiple_selections=false);
  bool SelectSurfacePoints(int region[4],
    vtkCollection* selectedRepresentations,
    vtkCollection* selectionSources,
    bool multiple_selections=false);

//  vtkSMProxy* SelectFrustumCells(int region[4]);
//  vtkSMProxy* SelectFrustumPoints(int region[4]);

//BTX
protected:
  vtkSMRenderView2Proxy();
  ~vtkSMRenderView2Proxy();

  // Description:
  // Fetches the LastSelection from the data-server and then converts it to a
  // selection source proxy and returns that.
  bool FetchLastSelection(vtkCollection* selectedRepresentations,
    vtkCollection* selectionSources);

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

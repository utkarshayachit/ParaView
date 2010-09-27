/*=========================================================================

  Program:   ParaView
  Module:    vtkSMRenderViewProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMRenderViewProxy - implementation for View that includes
// render window and renderers.
// .SECTION Description
// vtkSMRenderViewProxy is a 3D view consisting for a render window and two
// renderers: 1 for 3D geometry and 1 for overlayed 2D geometry.

#ifndef __vtkSMRenderViewProxy_h
#define __vtkSMRenderViewProxy_h

#include "vtkSMViewProxy.h"

class vtkCollection;
class vtkPVGenericRenderWindowInteractor;
class vtkRenderer;
class vtkRenderWindow;

class VTK_EXPORT vtkSMRenderViewProxy : public vtkSMViewProxy
{
public:
  static vtkSMRenderViewProxy* New();
  vtkTypeRevisionMacro(vtkSMRenderViewProxy, vtkSMViewProxy);
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

  // Description:
  // Returns true if it's possible to do GPU-based cell selection.
  bool IsSelectVisibleCellsAvailable() { return true; }

  // Description:
  // Returns true if it's possible to do GPU-based cell selection.
  bool IsSelectVisiblePointsAvailable() { return true; }

  // Description:
  // Returns the interactor.
  vtkPVGenericRenderWindowInteractor* GetInteractor();

  // Description:
  // Returns the client-side render window.
  vtkRenderWindow* GetRenderWindow();

  // Description:
  // Returns the client-side renderer (composited or 3D).
  vtkRenderer* GetRenderer();

//  vtkSMProxy* SelectFrustumCells(int region[4]);
//  vtkSMProxy* SelectFrustumPoints(int region[4]);

  // Description:
  // Create a default representation for the given source proxy.
  // Returns a new proxy.
  virtual vtkSMRepresentationProxy* CreateDefaultRepresentation(
    vtkSMProxy*, int);

//BTX
protected:
  vtkSMRenderViewProxy();
  ~vtkSMRenderViewProxy();

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
  vtkSMRenderViewProxy(const vtkSMRenderViewProxy&); // Not implemented
  void operator=(const vtkSMRenderViewProxy&); // Not implemented
//ETX
};

#endif

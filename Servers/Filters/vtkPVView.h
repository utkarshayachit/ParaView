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
// .NAME vtkPVView - baseclass for all ParaView views.
// .SECTION Description
// vtkPVView adds API to vtkView for ParaView specific views. Typically, one
// writes a simple vtkView subclass for their custom view. Then one subclasses
// vtkPVView to use their own vtkView subclass with added support for
// parallel rendering, tile-displays and client-server. Even if the view is
// client-only view, it needs to address these other configuration gracefully.

#ifndef __vtkPVView_h
#define __vtkPVView_h

#include "vtkView.h"
#include "vtkWeakPointer.h"

class vtkPVSynchronizedRenderWindows;
class VTK_EXPORT vtkPVView : public vtkView
{
public:
  vtkTypeMacro(vtkPVView, vtkView);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Initialize the view with an identifier. Unless noted otherwise, this method
  // must be called before calling any other methods on this class.
  // @CallOnAllProcessess
  virtual void Initialize(unsigned int id);

  // Description:
  // Set the position on this view in the multiview configuration.
  // This can be called only after Initialize().
  // @CallOnAllProcessess
  virtual void SetPosition(int, int);

  // Description:
  // Set the size of this view in the multiview configuration.
  // This can be called only after Initialize().
  // @CallOnAllProcessess
  virtual void SetSize(int, int);

  // Description:
  // Triggers a high-resolution render.
  // @CallOnAllProcessess
  virtual void StillRender()=0;

  // Description:
  // Triggers a interactive render. Based on the settings on the view, this may
  // result in a low-resolution rendering or a simplified geometry rendering.
  // @CallOnAllProcessess
  virtual void InteractiveRender()=0;

  // Description:
  // This encapsulates a whole lot of logic for
  // communication between processes. Given the ton of information this class
  // keeps, it can easily aid vtkViews to synchronize information such as bounds/
  // data-size among all processes efficiently. This can be achieved by using
  // these methods.
  // Note that these methods should be called on all processes at the same time
  // otherwise we will have deadlocks.
  // We may make this API generic in future, for now this works.
  bool SynchronizeBounds(double bounds[6]);
  bool SynchronizeSize(unsigned long &size);

  // Description:
  // Get/Set the time this view is showing.
  vtkSetMacro(ViewTime, double);
  vtkGetMacro(ViewTime, double);

//BTX
protected:
  vtkPVView();
  ~vtkPVView();

  // Description:
  // Returns true if the application is currently in tile display mode.
  bool InTileDisplayMode();

  // vtkPVSynchronizedRenderWindows is used to ensure that this view participates
  // in tile-display configurations. Even if your view subclass a simple
  // Qt-based client-side view that does not render anything on the
  // tile-display, it needs to be "registered with the
  // vtkPVSynchronizedRenderWindows so that the layout on the tile-displays for
  // other views shows up correctly. Ideally you'd want to paste some image on
  // the tile-display, maybe just a capture of the image rendering on the
  // client.
  // If your view needs a vtkRenderWindow, don't directly create it, always get
  // using vtkPVSynchronizedRenderWindows::NewRenderWindow().
  vtkPVSynchronizedRenderWindows* SynchronizedWindows;

  // Description:
  // Every view gets a unique identifier that it uses to register itself with
  // the SynchronizedWindows. This is set in Initialize().
  unsigned int Identifier;

  double ViewTime;

private:
  vtkPVView(const vtkPVView&); // Not implemented
  void operator=(const vtkPVView&); // Not implemented

  static vtkWeakPointer<vtkPVSynchronizedRenderWindows>
    SingletonSynchronizedWindows;

//ETX
};

#endif


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
// .NAME vtkPVRenderView
// .SECTION Description
// vtkRenderViewBase equivalent that is specialized for ParaView. Eventually
// vtkRenderViewBase should have a abstract base-class that this will derive from
// instead of vtkRenderViewBase since we do not use the labelling/icon stuff from
// vtkRenderViewBase.

// FIXME: Following is temporary -- until I decide if that's necessary at all.
// vtkPVRenderView has two types of public methods:
// 1. @CallOnAllProcessess -- must be called on all processes with exactly the
//                            same values.
// 2. @CallOnClientOnly    -- can be called only on the "client" process. These
//                            typically encapsulate client-side logic such as
//                            deciding if we are doing remote rendering or local
//                            rendering etc.

// Utkarsh: Try to use methods that will be called on all processes for most
// decision making similar to what ResetCamera() does. This will avoid the need
// to have special code in vtkSMRenderViewProxy and will simplify life when
// creating new views. "Move logic to VTK" -- that's the Mantra.
#ifndef __vtkPVRenderView_h
#define __vtkPVRenderView_h

#include "vtkPVView.h"

class vtkBSPCutsGenerator;
class vtkCamera;
class vtkInformationIntegerKey;
class vtkInformationObjectBaseKey;
class vtkInformationRequestKey;
class vtkPVSynchronizedRenderer;
class vtkPVSynchronizedRenderWindows;
class vtkRenderer;
class vtkRenderViewBase;
class vtkRenderWindow;

class VTK_EXPORT vtkPVRenderView : public vtkPVView
{
public:
  static vtkPVRenderView* New();
  vtkTypeMacro(vtkPVRenderView, vtkPVView);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Initialize the view with an identifier. Unless noted otherwise, this method
  // must be called before calling any other methods on this class.
  // @CallOnAllProcessess
  virtual void Initialize(unsigned int id);

  // Description:
  // Gets the non-composited renderer for this view. This is typically used for
  // labels, 2D annotations etc.
  // @CallOnAllProcessess
  vtkGetObjectMacro(NonCompositedRenderer, vtkRenderer);
  vtkRenderer* GetRenderer();

  // Description:
  // Get the active camera.
  vtkCamera* GetActiveCamera();

  // Description:
  // Returns the render window.
  vtkRenderWindow* GetRenderWindow();

  // Description:
  // Resets the active camera using collective prop-bounds.
  // @CallOnAllProcessess
  void ResetCamera();

  // Description:
  // Triggers a high-resolution render.
  // @CallOnAllProcessess
  virtual void StillRender();

  // Description:
  // Triggers a interactive render. Based on the settings on the view, this may
  // result in a low-resolution rendering or a simplified geometry rendering.
  // @CallOnAllProcessess
  virtual void InteractiveRender();

  // Description:
  // Get/Set the reduction-factor to use when for StillRender(). This is
  // typically set to 1, but in some cases with terrible connectivity or really
  // large displays, one may want to use a sub-sampled image even for
  // StillRender(). This is set it number of pixels to be sub-sampled by.
  // Note that image reduction factors have no effect when in built-in mode.
  // @CallOnAllProcessess
  vtkSetClampMacro(StillRenderImageReductionFactor, int, 1, 20);
  vtkGetMacro(StillRenderImageReductionFactor, int);

  // Description:
  // Get/Set the reduction-factor to use when for InteractiveRender().
  // This is set it number of pixels to be sub-sampled by.
  // Note that image reduction factors have no effect when in built-in mode.
  // @CallOnAllProcessess
  vtkSetClampMacro(InteractiveRenderImageReductionFactor, int, 1, 20);
  vtkGetMacro(InteractiveRenderImageReductionFactor, int);

  // Description:
  // Get/Set the data-size in kilobytes above which remote-rendering should be
  // used, if possible.
  // @CallOnAllProcessess
  vtkSetMacro(RemoteRenderingThreshold, unsigned long);
  vtkGetMacro(RemoteRenderingThreshold, unsigned long);

  // Description:
  // Get/Set the data-size in kilobytes above which LOD rendering should be
  // used, if possible.
  // @CallOnAllProcessess
  vtkSetMacro(LODRenderingThreshold, unsigned long);
  vtkGetMacro(LODRenderingThreshold, unsigned long);

  // Description:
  // This threshold is only applicable when in tile-display mode. It is the size
  // of geometry in kilobytes beyond which the view should not deliver geometry
  // to the client, but only outlines.
  // @CallOnAllProcessess
  vtkSetMacro(ClientOutlineThreshold, unsigned long);
  vtkGetMacro(ClientOutlineThreshold, unsigned long);

  // Description:
  // Resets the clipping range. One does not need to call this directly ever. It
  // is called periodically by the vtkRenderer to reset the camera range.
  void ResetCameraClippingRange();

  // Description:
  // Overridden to collect information for ordered-compositing.
  virtual void Update();

  // Description:
  // vtkDataRepresentation can use this key to publish meta-data about geometry
  // size in the VIEW_REQUEST_METADATA pass. If this meta-data is available,
  // then the view can make informed decisions about where to render/whether to
  // use LOD etc.
  static vtkInformationIntegerKey* GEOMETRY_SIZE();

  static vtkInformationIntegerKey* DATA_DISTRIBUTION_MODE();
  static vtkInformationIntegerKey* USE_LOD();
  static vtkInformationIntegerKey* DELIVER_LOD_TO_CLIENT();
  static vtkInformationIntegerKey* DELIVER_OUTLINE_TO_CLIENT();
  static vtkInformationIntegerKey* LOD_RESOLUTION();

  // Description:
  // This view supports ordered compositing, if needed. When ordered compositing
  // needs to be employed, this view requires that all representations
  // redistribute the data using a KdTree. To tell the view of the vtkAlgorithm
  // that is producing some redistributable data, representation can use this
  // key in their REQUEST_INFORMATION() pass to put the producer in the
  // outInfo.
  static vtkInformationObjectBaseKey* REDISTRIBUTABLE_DATA_PRODUCER();

  // Description:
  // Representation can publish this key in their REQUEST_INFORMATION() pass to
  // indicate that the representation needs ordered compositing.
  static vtkInformationIntegerKey* NEED_ORDERED_COMPOSITING();

  // Description:
  // This is used by the view in the REQUEST_RENDER() pass to tell the
  // representations the KdTree, if any to use for distributing the data. If
  // none is present, then representations should not redistribute the data.
  static vtkInformationObjectBaseKey* KD_TREE();

//BTX
protected:
  vtkPVRenderView();
  ~vtkPVRenderView();

  // Description:
  // Actual render method.
  void Render(bool interactive);

  // Description:
  // Calls vtkView::REQUEST_INFORMATION() on all representations
  void GatherRepresentationInformation();

  // Description:
  // Sychronizes the geometry size information on all nodes.
  // @CallOnAllProcessess
  void GatherGeometrySizeInformation();

  // Description:
  // Returns true if distributed rendering should be used.
  bool GetUseDistributedRendering();

  // Description:
  // Returns true if LOD rendering should be used.
  bool GetUseLODRendering();

  // Description:
  // Returns true when ordered compositing is needed on the current group of
  // processes. Note that unlike most other functions, this may return different
  // values on different processes e.g.
  // \li always false on client and dataserver
  // \li true on pvserver or renderserver if opacity < 1 or volume present, else
  //     false
  bool GetUseOrderedCompositing();

  // Description:
  // Returns true if outline should be delivered to client.
  bool GetDeliverOutlineToClient();

  // Description:
  // Update the request to enable/disable distributed rendering.
  void SetRequestDistributedRendering(bool);

  // Description:
  // Update the request to enable/disable low-res rendering.
  void SetRequestLODRendering(bool);

  vtkRenderViewBase* RenderView;
  vtkRenderer* NonCompositedRenderer;
  vtkPVSynchronizedRenderer* SynchronizedRenderers;

  int StillRenderImageReductionFactor;
  int InteractiveRenderImageReductionFactor;

  unsigned long LocalGeometrySize;
  unsigned long GeometrySize;
  unsigned long RemoteRenderingThreshold;
  unsigned long LODRenderingThreshold;
  unsigned long ClientOutlineThreshold;
  double LastComputedBounds[6];

  vtkBSPCutsGenerator* OrderedCompositingBSPCutsSource;
private:
  vtkPVRenderView(const vtkPVRenderView&); // Not implemented
  void operator=(const vtkPVRenderView&); // Not implemented
//ETX
};

#endif

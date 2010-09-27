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
class vtkInformationDoubleKey;
class vtkInformationIntegerKey;
class vtkInformationObjectBaseKey;
class vtkInformationRequestKey;
class vtkInteractorStyleRubberBand3D;
class vtkLight;
class vtkLightKit;
class vtkPHardwareSelector;
class vtkProp;
class vtkPVAxesWidget;
class vtkPVCenterAxesActor;
class vtkPVGenericRenderWindowInteractor;
class vtkPVInteractorStyle;
class vtkPVSynchronizedRenderer;
class vtkPVSynchronizedRenderWindows;
class vtkRenderer;
class vtkRenderViewBase;
class vtkRenderWindow;
class vtkTexture;

class VTK_EXPORT vtkPVRenderView : public vtkPVView
{
  //*****************************************************************
public:
  static vtkPVRenderView* New();
  vtkTypeMacro(vtkPVRenderView, vtkPVView);
  void PrintSelf(ostream& os, vtkIndent indent);

  enum InteractionModes
    {
    INTERACTION_MODE_3D=0,
    INTERACTION_MODE_2D, // not implemented yet.
    INTERACTION_MODE_FRUSTUM_SELECTION,
    INTERACTION_MODE_SURFACE_SELECTION
    };

  // Description:
  // Get/Set the interaction mode. Default is INTERACTION_MODE_3D. If
  // INTERACTION_MODE_FRUSTUM_SELECTION or INTERACTION_MODE_SURFACE_SELECTION is
  // selected, then whenever the user drags and creates a selection-region, an
  // appropriate selection will be created and vtkCommand::SelectionChangedEvent
  // will be fired.
  // @CallOnAllProcessess - this must be called on all processes, however it will
  // have any effect only the driver processes i.e. the process with the
  // interactor.
  void SetInteractionMode(int mode);
  vtkGetMacro(InteractionMode, int);

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
  // Get/Set the active camera. The active camera is set on both the composited
  // and non-composited renderer.
  vtkCamera* GetActiveCamera();
  virtual void SetActiveCamera(vtkCamera*);

  // Description:
  // Returns the render window.
  vtkRenderWindow* GetRenderWindow();

  // Description:
  // It's possible to directly add vtkProps to a view. This API provides access
  // to add props to the 3D/composited renderer. Note that if you add props in
  // this way, they will not be included in the computations for geometry-size
  // which is used to make decisions whether to use LOD or remote rendering etc.
  // Nor can such props participate in data-redistribution when volume rendering
  // or translucent rendering. As a rule of thumb only add props not directly
  // connected to input data using this API such as scalar bars, cube axes etc.
  void AddPropToRenderer(vtkProp*);
  void RemovePropFromRenderer(vtkProp*);

  // Description:
  // It's possible to directly add vtkProps to a view. This API provides access
  // to add props to the non-composited renderer. Note that if you add props in
  // this way, they will not be included in the computations for geometry-size
  // which is used to make decisions whether to use LOD or remote rendering etc.
  // Nor can such props participate in data-redistribution when volume rendering
  // or translucent rendering. As a rule of thumb only add props not directly
  // connected to input data using this API such as scalar bars, cube axes etc.
  void AddPropToNonCompositedRenderer(vtkProp*);
  void RemovePropFromNonCompositedRenderer(vtkProp*);

  // Description:
  // Returns the interactor. .
  vtkGetObjectMacro(Interactor, vtkPVGenericRenderWindowInteractor);

  // Description:
  // Returns the interactor style.
  vtkGetObjectMacro(InteractorStyle, vtkPVInteractorStyle);

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
  // Get/Set the LOD resolution. This affects the size of the grid used for
  // quadric clustering, for example. 1.0 implies maximum resolution while 0
  // implies minimum resolution.
  // @CallOnAllProcessess
  vtkSetClampMacro(LODResolution, double, 0.0, 1.0);
  vtkGetMacro(LODResolution, double);

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
  // Enable/Disable light kit.
  // @CallOnAllProcessess
  void SetUseLightKit(bool enable);
  vtkGetMacro(UseLightKit, bool);
  vtkBooleanMacro(UseLightKit, bool);

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
  static vtkInformationDoubleKey* LOD_RESOLUTION();

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

  // Description:
  // Make a selection. This will result in setting up of this->LastSelection
  // which can be accessed using GetLastSelection().
  // @CallOnAllProcessess
  void SelectCells(int region[4]);
  void SelectPoints(int region[4]);
  void Select(int field_association, int region[4]);

  // Description:
  // Provides access to the last selection.
  vtkGetObjectMacro(LastSelection, vtkSelection);

public:
  //*****************************************************************
  // Methods merely exposing methods for internal objects.

  // Description:
  // Turn on/off the default light in the 3D renderer.
  void SetLightSwitch(bool enable);
  bool GetLightSwitch();
  vtkBooleanMacro(LightSwitch, bool);

  //*****************************************************************
  // Forwarded to orientation axes widget.
  void SetOrientationAxesInteractivity(bool);
  void SetOrientationAxesVisibility(bool);
  void SetOrientationAxesLabelColor(double r, double g, double b);
  void SetOrientationAxesOutlineColor(double r, double g, double b);

  //*****************************************************************
  // Forwarded to center axes.
  void SetCenterAxesVisibility(bool);

  //*****************************************************************
  // Forward to vtkPVGenericRenderWindowInteractor.
  void SetCenterOfRotation(double x, double y, double z);

  //*****************************************************************
  // Forward to vtkLightKit.
  void SetKeyLightWarmth(double val);
  void SetKeyLightIntensity(double val);
  void SetKeyLightElevation(double val);
  void SetKeyLightAzimuth(double val);
  void SetFillLightWarmth(double val);
  void SetKeyToFillRatio(double val);
  void SetFillLightElevation(double val);
  void SetFillLightAzimuth(double val);
  void SetBackLightWarmth(double val);
  void SetKeyToBackRatio(double val);
  void SetBackLightElevation(double val);
  void SetBackLightAzimuth(double val);
  void SetHeadLightWarmth(double val);
  void SetKeyToHeadRatio(double val);
  void SetMaintainLuminance(int val);

  //*****************************************************************
  // Forward to 3D renderer.
  void SetUseDepthPeeling(int val);
  void SetMaximumNumberOfPeels(int val);
  void SetBackground(double r, double g, double b);
  void SetBackground2(double r, double g, double b);
  void SetBackgroundTexture(vtkTexture* val);
  void SetGradientBackground(int val);
  void SetTexturedBackground(int val);

  //*****************************************************************
  // Forward to vtkLight.
  void SetAmbientColor(double r, double g, double b);
  void SetSpecularColor(double r, double g, double b);
  void SetDiffuseColor(double r, double g, double b);
  void SetIntensity(double val);
  void SetLightType(int val);

  //*****************************************************************
  // Forward to vtkRenderWindow.
  void SetStereoCapableWindow(int val);
  void SetStereoRender(int val);
  void SetStereoType(int val);
  void SetOffScreenRendering(int val);
  void SetMultiSamples(int val);
  void SetAlphaBitPlanes(int val);
  void SetStencilCapable(int val);


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
  // Synchronizes bounds information on all nodes.
  // @CallOnAllProcessess
  void GatherBoundsInformation();

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

  // Description:
  // Set the last selection object.
  void SetLastSelection(vtkSelection*);

  // Description:
  // UpdateCenterAxes().
  // Updates CenterAxes's scale and position.
  void UpdateCenterAxes(double bounds[6]);

  vtkLight* Light;
  vtkLightKit* LightKit;
  vtkRenderViewBase* RenderView;
  vtkRenderer* NonCompositedRenderer;
  vtkPVSynchronizedRenderer* SynchronizedRenderers;
  vtkPVGenericRenderWindowInteractor* Interactor;
  vtkPVInteractorStyle* InteractorStyle;
  vtkInteractorStyleRubberBand3D* RubberBandStyle;
  vtkPVCenterAxesActor* CenterAxes;
  vtkPVAxesWidget* OrientationWidget;
  vtkPHardwareSelector* Selector;
  vtkSelection* LastSelection;

  int StillRenderImageReductionFactor;
  int InteractiveRenderImageReductionFactor;
  int InteractionMode;

  unsigned long LocalGeometrySize;
  unsigned long GeometrySize;
  unsigned long RemoteRenderingThreshold;
  unsigned long LODRenderingThreshold;
  unsigned long ClientOutlineThreshold;
  double LastComputedBounds[6];

  double LODResolution;
  bool UseLightKit;

  vtkBSPCutsGenerator* OrderedCompositingBSPCutsSource;
private:
  vtkPVRenderView(const vtkPVRenderView&); // Not implemented
  void operator=(const vtkPVRenderView&); // Not implemented

  void OnSelectionChangedEvent();
  void FinishSelection();
  void FinishSelection(vtkSelection*);
//ETX
};

#endif

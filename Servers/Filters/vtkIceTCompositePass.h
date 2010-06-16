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
// .NAME vtkIceTCompositePass - vtkRenderPass subclass for compositing
// renderings across processes using IceT.
// .SECTION Description
// vtkIceTCompositePass is a vtkRenderPass subclass that can be used for
// compositing images (rgba or depth buffer) across processes using IceT.
// This can be used in lieu of vtkCompositeRGBAPass. The usage of this pass
// differs slightly from vtkCompositeRGBAPass. vtkCompositeRGBAPass composites
// the active frame buffer, while this class requires that the render pass to
// render info the frame buffer that needs to be composited should be set as an
// ivar using SetRenderPass().
//
// This class also provides support for tile-displays. Simply set the
// TileDimensions > [1, 1] and instead of rendering a composited image
// on the root node, it will split the view among all tiles and generate
// renderings on all processes.

#ifndef __vtkIceTCompositePass_h
#define __vtkIceTCompositePass_h

#include "vtkRenderPass.h"
#include "vtkSynchronizedRenderers.h" //  needed for vtkRawImage.

class vtkMultiProcessController;
class vtkPKdTree;
class vtkIceTContext;

class VTK_EXPORT vtkIceTCompositePass : public vtkRenderPass
{
public:
  static vtkIceTCompositePass* New();
  vtkTypeRevisionMacro(vtkIceTCompositePass, vtkRenderPass);
  void PrintSelf(ostream& os, vtkIndent indent);
//BTX
  // Description:
  // Perform rendering according to a render state \p s.
  // \pre s_exists: s!=0
  virtual void Render(const vtkRenderState *s);
//ETX

  // Description:
  // Release graphics resources and ask components to release their own
  // resources.
  // \pre w_exists: w!=0
  void ReleaseGraphicsResources(vtkWindow *w);

  // Description:
  // Controller
  // If it is NULL, nothing will be rendered and a warning will be emitted.
  // Initial value is a NULL pointer.
  vtkGetObjectMacro(Controller,vtkMultiProcessController);
  virtual void SetController(vtkMultiProcessController *controller);

  // Description:
  // Get/Set the render pass used to do the actual rendering. The result of this
  // delete pass is what gets composited using IceT.
  void SetRenderPass(vtkRenderPass*);
  vtkGetObjectMacro(RenderPass, vtkRenderPass);

  // Description:
  // Get/Set the tile dimensions. Default is (1, 1). If any of the dimensions is
  // > 1 than tile display mode is assumed.
  vtkSetVector2Macro(TileDimensions, int);
  vtkGetVector2Macro(TileDimensions, int);

  // Description:
  // Get/Set the tile mullions. The mullions are measured in pixels. Use
  // negative numbers for overlap.
  vtkSetVector2Macro(TileMullions, int);
  vtkGetVector2Macro(TileMullions, int);

  // Description:
  // Set to true if data is replicated on all processes. This will enable IceT
  // to minimize communications since data is available on all process. Off by
  // default.
  vtkSetMacro(DataReplicatedOnAllProcesses, bool);
  vtkGetMacro(DataReplicatedOnAllProcesses, bool);
  vtkBooleanMacro(DataReplicatedOnAllProcesses, bool);

  // Description:
  // Set the image reduction factor. This can be used to speed up compositing.
  // When using vtkIceTCompositePass use this image reduction factor rather than
  // that on vtkSynchronizedRenderers since using
  // vtkSynchronizedRenderers::ImageReductionFactor will not work correctly with
  // IceT.
  vtkSetClampMacro(ImageReductionFactor, int, 1, 50);
  vtkGetMacro(ImageReductionFactor, int);

  // Description:
  // kd tree that gives processes ordering. Initial value is a NULL pointer.
  // This is used only when UseOrderedCompositing is true.
  vtkGetObjectMacro(KdTree,vtkPKdTree);
  virtual void SetKdTree(vtkPKdTree *kdtree);


  // Description:
  // Set this to true, if compositing must be done in a specific order. This is
  // necessary when rendering volumes or translucent geometries. When
  // UseOrderedCompositing is set to true, it is expected that the KdTree is set as
  // well. The KdTree is used to decide the process-order for compositing.
  vtkGetMacro(UseOrderedCompositing, bool);
  vtkSetMacro(UseOrderedCompositing, bool);
  vtkBooleanMacro(UseOrderedCompositing, bool);

//BTX
  // Description:
  // Internal callback. Don't call directly.
  void SetupContext(const vtkRenderState*);
  void Draw(const vtkRenderState*);
  void CleanupContext(const vtkRenderState*);

  // Description:
  // Returns the last rendered tile from this process, if any.
  // Image is invalid if tile is not available on the current process.
  void GetLastRenderedTile(vtkSynchronizedRenderers::vtkRawImage& tile);

  void IceTInflateAndDisplay(vtkRenderer*);
protected:
  vtkIceTCompositePass();
  ~vtkIceTCompositePass();

  void UpdateTileInformation(const vtkRenderState*);

  vtkMultiProcessController *Controller;
  vtkPKdTree *KdTree;
  vtkRenderPass* RenderPass;
  vtkIceTContext* IceTContext;

  bool UseOrderedCompositing;
  bool DataReplicatedOnAllProcesses;
  int TileDimensions[2];
  int TileMullions[2];

  int LastTileDimensions[2];
  int LastTileMullions[2];
  int LastTileViewport[4];
  double PhysicalViewport[4];

  int ImageReductionFactor;
private:
  vtkIceTCompositePass(const vtkIceTCompositePass&); // Not implemented
  void operator=(const vtkIceTCompositePass&); // Not implemented
//ETX
};

#endif

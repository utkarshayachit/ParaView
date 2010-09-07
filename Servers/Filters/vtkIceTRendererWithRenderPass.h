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
// .NAME vtkIceTRendererWithRenderPass
// .SECTION Description
// vtkIceTRendererWithRenderPass extends vtkIceTRenderer to add support for
// render passes.

#ifndef __vtkIceTRendererWithRenderPass_h
#define __vtkIceTRendererWithRenderPass_h

#include "vtkIceTRenderer.h"
class vtkIceTCompositePass;

class VTK_EXPORT vtkIceTRendererWithRenderPass : public vtkIceTRenderer
{
public:
  static vtkIceTRendererWithRenderPass* New();
  vtkTypeMacro(vtkIceTRendererWithRenderPass, vtkIceTRenderer);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Get/Set the render-pass used to render the geometry for every ice-t
  // callback. Default is NULL in which case the default vtkOpenGLRenderer
  // behavior is provided. To override that, set this to a custom pass.
  void SetCompositedPass(vtkRenderPass*);
  vtkGetObjectMacro(CompositedPass, vtkRenderPass);

  void SetTileDimensions(int tilesX, int tilesY);
  void SetTileMullions(int mullX, int mullY);

  // Description:
  // Overridden to capture the image from the renderer buffers itself, rather
  // than icet buffers, since render passes may have rendered over what ice-t
  // delivered.
  virtual bool RecordIceTImage(vtkUnsignedCharArray* buffer,
    int image_width, int image_height);
//BTX
protected:
  vtkIceTRendererWithRenderPass();
  ~vtkIceTRendererWithRenderPass();

  vtkRenderPass* CompositedPass;
  vtkIceTCompositePass* IceTCompositePass;

private:
  vtkIceTRendererWithRenderPass(const vtkIceTRendererWithRenderPass&); // Not implemented
  void operator=(const vtkIceTRendererWithRenderPass&); // Not implemented
//ETX
};

#endif

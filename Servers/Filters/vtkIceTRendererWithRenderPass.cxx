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
#include "vtkIceTRendererWithRenderPass.h"

#include "vtkCameraPass.h"
#include "vtkIceTCompositePass.h"
#include "vtkIceTContext.h"
#include "vtkObjectFactory.h"
#include "vtkRenderPass.h"
#include "vtkRenderState.h"
#include "vtkRenderWindow.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"

#include <vtkgl.h>
#include <GL/ice-t.h>

namespace
{
  void IceTDrawCallback();

  class vtkMyCameraPass : public vtkCameraPass
  {
public:
  vtkTypeMacro(vtkMyCameraPass, vtkCameraPass);
  static vtkMyCameraPass* New();

  virtual void GetTiledSizeAndOrigin(
    const vtkRenderState* render_state,
    int* width, int* height, int *originX,
    int* originY)
    {
    int tile_dims[2];
    this->IceTCompositePass->GetTileDimensions(tile_dims);
    if (tile_dims[0] > 1 || tile_dims[1]  > 1)
      {
      // we have a complicated relationship with tile-scale when we are in
      // tile-display mode :).
      // vtkPVSynchronizedRenderWindows sets up the tile-scale and origin on the
      // window so that 2D annotations work just fine. However that messes up
      // when we are using IceT since IceT will do the camera translations. So
      // for IceT's sake, we reset the tile_scale/tile_viewport when doing the
      // camera transformations. Of course, this is only an issue when rendering
      // for tile-displays.
      int tile_scale[2];
      double tile_viewport[4];
      render_state->GetRenderer()->GetRenderWindow()->GetTileScale(tile_scale);
      render_state->GetRenderer()->GetRenderWindow()->GetTileViewport(tile_viewport);
      render_state->GetRenderer()->GetRenderWindow()->SetTileScale(1, 1);
      render_state->GetRenderer()->GetRenderWindow()->SetTileViewport(0,0,1,1);
      this->Superclass::GetTiledSizeAndOrigin(render_state, width, height, originX, originY);
      render_state->GetRenderer()->GetRenderWindow()->SetTileScale(tile_scale);
      render_state->GetRenderer()->GetRenderWindow()->SetTileViewport(tile_viewport);

      *originX *= this->IceTCompositePass->GetTileDimensions()[0];
      *originY *= this->IceTCompositePass->GetTileDimensions()[1];
      *width *= this->IceTCompositePass->GetTileDimensions()[0];
      *height *= this->IceTCompositePass->GetTileDimensions()[1];
      }
    else
      {
      this->Superclass::GetTiledSizeAndOrigin(render_state, width, height, originX, originY);
      }
    }

  vtkIceTCompositePass* IceTCompositePass;
  };

  vtkStandardNewMacro(vtkMyCameraPass);

  class vtkInitialPass : public vtkRenderPass
  {
public:
  vtkTypeMacro(vtkInitialPass, vtkRenderPass);
  static vtkInitialPass* New();
  virtual void Render(const vtkRenderState *s)
    {
    vtkRenderer* renderer = s->GetRenderer();
    vtkRenderWindow* window = renderer->GetRenderWindow();

    // CODE COPIED FROM vtkOpenGLRenderer.
    // Oh! How I hate such kind of copying, sigh :(.
    vtkTimerLog::MarkStartEvent("vtkInitialPass::Render");

    // Do not remove this MakeCurrent! Due to Start / End methods on
    // some objects which get executed during a pipeline update,
    // other windows might get rendered since the last time
    // a MakeCurrent was called.
    window->MakeCurrent();

    // Don't do any geometry-related rendering just yet. That needs to be done
    // in the icet callback.
    this->IceTCompositePass->SetupContext(s);

    // Don't make icet render the composited image to the screen. We'll paste it
    // explicitly if needed. This is required since IceT/Viewport interactions
    // lead to weird results in multi-view configurations. Much easier to simply
    // paste back the image to the correct region after icet has rendered.
    icetEnable(ICET_DISPLAY);
    icetDisable(ICET_DISPLAY_INFLATE);
    icetEnable(ICET_CORRECT_COLORED_BACKGROUND);

    int *size = window->GetActualSize();
    glViewport(0, 0, size[0], size[1]);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 0);

    icetDrawFunc(IceTDrawCallback);
    vtkInitialPass::ActiveRenderer = renderer;
    vtkInitialPass::ActivePass = this;
    icetDrawFrame();
    vtkInitialPass::ActiveRenderer = NULL;
    vtkInitialPass::ActivePass = NULL;

    this->IceTCompositePass->CleanupContext(s);
    vtkTimerLog::MarkEndEvent("vtkInitialPass::Render");
    }

  static void Draw()
    {
    if (vtkInitialPass::ActiveRenderer && vtkInitialPass::ActivePass)
      {
      vtkInitialPass::ActivePass->DrawInternal(vtkInitialPass::ActiveRenderer);
      }
    }

  vtkSetObjectMacro(IceTCompositePass, vtkIceTCompositePass);
protected:
  static vtkInitialPass* ActivePass;
  static vtkRenderer* ActiveRenderer;

  vtkInitialPass()
    {
    this->IceTCompositePass = 0;
    }

  ~vtkInitialPass()
    {
    this->SetIceTCompositePass(0);
    }

  void DrawInternal(vtkRenderer* ren)
    {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    this->ClearLights(ren);
    this->UpdateLightGeometry(ren);
    this->UpdateLights(ren);

    //// set matrix mode for actors
    glMatrixMode(GL_MODELVIEW);

    this->UpdateGeometry(ren);
    }

  vtkIceTCompositePass* IceTCompositePass;
  };

  vtkInitialPass* vtkInitialPass::ActivePass = NULL;
  vtkRenderer* vtkInitialPass::ActiveRenderer = NULL;
  vtkStandardNewMacro(vtkInitialPass);


  void IceTDrawCallback()
    {
    vtkInitialPass::Draw();
    }
};

#include "vtkSobelGradientMagnitudePass.h"

vtkStandardNewMacro(vtkIceTRendererWithRenderPass);
vtkCxxSetObjectMacro(vtkIceTRendererWithRenderPass,
  CompositedPass, vtkRenderPass);
//----------------------------------------------------------------------------
vtkIceTRendererWithRenderPass::vtkIceTRendererWithRenderPass()
{
  this->CompositedPass = 0;

  this->IceTCompositePass = vtkIceTCompositePass::New();
  this->IceTCompositePass->IceTContext->Delete();
  this->IceTCompositePass->IceTContext = this->Context;
  this->IceTCompositePass->IceTContext->Register(this);
  this->IceTCompositePass->SetController(this->Context->GetController());

  // vtkIceTRenderManager handles setting up of the icet-tiles. So we skip that
  // when IceTCompositePass is rendering.
  this->IceTCompositePass->IceTTilesExternallyInitialized = true;

  vtkInitialPass* initPass = vtkInitialPass::New();
  initPass->SetIceTCompositePass(this->IceTCompositePass);

  vtkMyCameraPass* cameraPass = vtkMyCameraPass::New();
  cameraPass->IceTCompositePass = this->IceTCompositePass;
  cameraPass->SetDelegatePass(initPass);
  initPass->Delete();

  // This results in pretty much overriding the whole of the superclass code
  // since vtkRenderer skips calling this->DeviceRender() when this->Pass is
  // set.
  this->SetPass(cameraPass);
  cameraPass->Delete();

  vtkSobelGradientMagnitudePass *sobelPass =
    vtkSobelGradientMagnitudePass::New();
  sobelPass->SetDelegatePass(this->GetPass());
  this->SetPass(sobelPass);
  sobelPass->Delete();
}

//----------------------------------------------------------------------------
vtkIceTRendererWithRenderPass::~vtkIceTRendererWithRenderPass()
{
  this->SetCompositedPass(0);
  this->SetPass(0);

  this->IceTCompositePass->Delete();
  this->IceTCompositePass = 0;
}

//----------------------------------------------------------------------------
void vtkIceTRendererWithRenderPass::SetTileDimensions(int tilesX, int tilesY)
{
  this->IceTCompositePass->SetTileDimensions(tilesX>0? tilesX : 1,
    tilesY>0? tilesY: 1);
}

//----------------------------------------------------------------------------
void vtkIceTRendererWithRenderPass::SetTileMullions(int mullX, int mullY)
{
  this->IceTCompositePass->SetTileMullions(mullX, mullY);
}

//----------------------------------------------------------------------------
bool vtkIceTRendererWithRenderPass::RecordIceTImage(vtkUnsignedCharArray* buffer,
  int buffer_width, int buffer_height)
{
  int physicalViewport[4];
  this->GetPhysicalViewport(physicalViewport);

  int width  = physicalViewport[2] - physicalViewport[0];
  int height = physicalViewport[3] - physicalViewport[1];

  // See if this renderer is displaying anything in this tile.
  if (width < 1 || height < 1)
    {
    return false;
    }

  vtkUnsignedCharArray* screencapture = vtkUnsignedCharArray::New();
  screencapture->SetNumberOfComponents(4);
  screencapture->SetNumberOfTuples(width*height);

  this->GetRenderWindow()->GetRGBACharPixelData(
    physicalViewport[0], physicalViewport[1],
    physicalViewport[2]-1, physicalViewport[3]-1,
    this->GetRenderWindow()->GetDoubleBuffer()? 0 : 1,
    screencapture);

  // Copy as 4-bytes.  It's faster.
  GLuint *dest = (GLuint *)buffer->WritePointer(
    0, 4*buffer_width*buffer_height);
  GLuint *src = (GLuint *)screencapture->GetVoidPointer(0);
  dest += physicalViewport[1]*buffer_width;
  for (int j = 0; j < height; j++)
    {
    dest += physicalViewport[0];
    for (int i = 0; i < width; i++)
      {
      dest[0] = src[0];
      dest++;  src++;
      }
    dest += (buffer_width - physicalViewport[2]);
    }
  screencapture->Delete();
  return true;
}

//----------------------------------------------------------------------------
void vtkIceTRendererWithRenderPass::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

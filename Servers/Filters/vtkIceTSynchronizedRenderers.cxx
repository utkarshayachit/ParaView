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
#include "vtkIceTSynchronizedRenderers.h"

#include "vtkCameraPass.h"
#include "vtkCullerCollection.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkRenderState.h"
#include "vtkRenderWindow.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"

#include <vtkgl.h>
#include <GL/ice-t.h>

#include <vtkstd/map>

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
    icetDisable(ICET_DISPLAY);
    icetDisable(ICET_DISPLAY_INFLATE);
    icetDisable(ICET_CORRECT_COLORED_BACKGROUND);

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

  vtkIceTSynchronizedRenderers* IceTSynchronizedRenderers;
  vtkSetObjectMacro(IceTCompositePass, vtkIceTCompositePass);
protected:
  static vtkInitialPass* ActivePass;
  static vtkRenderer* ActiveRenderer;

  vtkInitialPass()
    {
    this->IceTCompositePass = 0;
    this->IceTSynchronizedRenderers = 0;
    }

  ~vtkInitialPass()
    {
    this->SetIceTCompositePass(0);
    this->IceTSynchronizedRenderers = 0;
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

  // We didn't want to have a singleton for vtkIceTSynchronizedRenderers to
  // manage multi-view configurations. But, in tile-display mode, after each
  // view is rendered, the tiles end up with the residue of that rendered view
  // on all tiles. Which is not what is expected -- one would expect the views
  // that are present on those tiles to be drawn back. That becomes tricky
  // without a singleton. So we have a internal map that tracks all rendered
  // tiles.
  class vtkTile
    {
  public:
    vtkSynchronizedRenderers::vtkRawImage TileImage;

    // PhysicalViewport is the viewport where the TileImage maps into the tile
    // rendered by this processes i.e. the render window for this process.
    double PhysicalViewport[4];

    // GlobalViewport is the viewport for this image treating all tiles as a
    // single large display.
    double GlobalViewport[4];
    };

  typedef vtkstd::map<vtkIceTSynchronizedRenderers*, vtkTile> TilesMapType;
  static TilesMapType TilesMap;

  // Iterates over all valid tiles in the TilesMap and flush the images to the
  // screen.
  void FlushTiles(vtkRenderer* renderer)
    {
    for (TilesMapType::iterator iter = ::TilesMap.begin();
      iter != :: TilesMap.end(); ++iter)
      {
      vtkTile& tile = iter->second;
      if (tile.TileImage.IsValid())
        {
        double viewport[4];
        renderer->GetViewport(viewport);
        renderer->SetViewport(tile.PhysicalViewport);
        int tile_scale[2];
        renderer->GetVTKWindow()->GetTileScale(tile_scale);
        renderer->GetVTKWindow()->SetTileScale(1, 1);
        tile.TileImage.PushToViewport(renderer);
        renderer->GetVTKWindow()->SetTileScale(tile_scale);
        renderer->SetViewport(viewport);
        }
      }
    }
};

vtkStandardNewMacro(vtkIceTSynchronizedRenderers);
//----------------------------------------------------------------------------
vtkIceTSynchronizedRenderers::vtkIceTSynchronizedRenderers()
{
  // First thing we do is create the ice-t render pass. This is essential since
  // most methods calls on this class simply forward it to the ice-t render
  // pass.
  this->IceTCompositePass = vtkIceTCompositePass::New();

  vtkInitialPass* initPass = vtkInitialPass::New();
  initPass->IceTSynchronizedRenderers = this;
  initPass->SetIceTCompositePass(this->IceTCompositePass);

  vtkMyCameraPass* cameraPass = vtkMyCameraPass::New();
  cameraPass->IceTCompositePass = this->IceTCompositePass;
  cameraPass->SetDelegatePass(initPass);
  initPass->Delete();

  this->RenderPass = cameraPass;
  this->SetParallelController(vtkMultiProcessController::GetGlobalController());
}

//----------------------------------------------------------------------------
vtkIceTSynchronizedRenderers::~vtkIceTSynchronizedRenderers()
{
  this->IceTCompositePass->Delete();
  this->IceTCompositePass = 0;
  this->RenderPass->Delete();
  this->RenderPass = 0;
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::HandleEndRender()
{
  if (this->WriteBackImages)
    {
    this->WriteBackImages = false;
    this->Superclass::HandleEndRender();
    this->WriteBackImages = true;
    }
  else
    {
    this->Superclass::HandleEndRender();
    }

  if (this->WriteBackImages)
    {
    vtkSynchronizedRenderers::vtkRawImage lastRenderedImage =
      this->CaptureRenderedImage();
    if (lastRenderedImage.IsValid())
      {
      vtkTile& tile = TilesMap[this];
      tile.TileImage = lastRenderedImage;
      // FIXME: Get real physcial viewport from vtkIceTCompositePass.
      this->IceTCompositePass->GetPhysicalViewport(tile.PhysicalViewport);
      }

    // Write-back either the freshly rendered tile or what was most recently
    // rendered.
    ::FlushTiles(this->Renderer);
    }
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::SetRenderer(vtkRenderer* ren)
{
  if (this->Renderer && this->Renderer->GetPass() == this->RenderPass)
    {
    this->Renderer->SetPass(NULL);
    }
  this->Superclass::SetRenderer(ren);
  if (ren)
    {
    ren->SetPass(this->RenderPass);
    // icet cannot work correctly in tile-display mode is software culling is
    // applied in vtkRenderer inself. vtkInitialPass will cull out-of-frustum
    // props using icet-model-view matrix later.
    ren->GetCullers()->RemoveAllItems();
    }
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::SetImageReductionFactor(int val)
{
  // Don't call superclass. Since ice-t has better mechanisms for dealing with
  // image reduction factor rather than simply reducing the viewport. This
  // ensures that it works nicely in tile-display mode as well.
  // this->Superclass::SetImageReductionFactor(val);
  this->IceTCompositePass->SetImageReductionFactor(val);
}

//----------------------------------------------------------------------------
vtkSynchronizedRenderers::vtkRawImage&
vtkIceTSynchronizedRenderers::CaptureRenderedImage()
{
  // We capture the image from IceTCompositePass. This avoids the capture of
  // buffer from screen when not necessary.
  vtkRawImage& rawImage =
    (this->GetImageReductionFactor() == 1)?
    this->FullImage : this->ReducedImage;

  if (!rawImage.IsValid())
    {
    this->IceTCompositePass->GetLastRenderedTile(rawImage);
    if (!rawImage.IsValid())
      {
      cout << "no image captured " << endl;
      //vtkErrorMacro("IceT couldn't provide a tile on this process.");
      }
    }
  return rawImage;
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

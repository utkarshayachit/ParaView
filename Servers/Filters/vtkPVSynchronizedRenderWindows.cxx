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
#include "vtkPVSynchronizedRenderWindows.h"

#include "vtkBoundingBox.h"
#include "vtkCommand.h"
#include "vtkMultiProcessStream.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVOptions.h"
#include "vtkPVServerInformation.h"
#include "vtkRemoteConnection.h"
#include "vtkRendererCollection.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkSelectionSerializer.h"
#include "vtkSmartPointer.h"
#include "vtkSocketController.h"
#include "vtkTilesHelper.h"

#include <vtksys/ios/sstream>
#include <vtkstd/map>
#include <vtkstd/vector>
#include <assert.h>

class vtkPVSynchronizedRenderWindows::vtkInternals
{
public:
  typedef vtkstd::vector<vtkSmartPointer<vtkRenderer> > VectorOfRenderers;

  struct RenderWindowInfo
    {
    int Size[2];
    int Position[2];
    unsigned long StartRenderTag;
    unsigned long EndRenderTag;
    vtkSmartPointer<vtkRenderWindow> RenderWindow;

    VectorOfRenderers Renderers;

    RenderWindowInfo()
      {
      this->Size[0] = this->Size[1] = 0;
      this->Position[0] = this->Position[1] = 0;
      this->StartRenderTag = this->EndRenderTag = 0;
      }
    };

  typedef vtkstd::map<unsigned int, RenderWindowInfo> RenderWindowsMap;
  RenderWindowsMap RenderWindows;

  unsigned int GetKey(vtkRenderWindow* win)
    {
    RenderWindowsMap::iterator iter;
    for (iter = this->RenderWindows.begin(); iter != this->RenderWindows.end();
      ++iter)
      {
      if (iter->second.RenderWindow == win)
        {
        return iter->first;
        }
      }
    return 0;
    }

  // Updates the viewport for all the renderers in the collection.
  void UpdateViewports(VectorOfRenderers& renderers, double viewport[4])
    {
    VectorOfRenderers::iterator iter;
    for (iter = renderers.begin(); iter != renderers.end(); ++iter)
      {
      (*iter)->SetViewport(viewport);
      }
    }

  vtkSmartPointer<vtkRenderWindow> SharedRenderWindow;
  unsigned int ActiveId;
};

//----------------------------------------------------------------------------
class vtkPVSynchronizedRenderWindows::vtkObserver : public vtkCommand
{
public:
  static vtkObserver* New()
    {
    vtkObserver* obs = new vtkObserver();
    obs->Target = NULL;
    return obs;
    }

  virtual void Execute(vtkObject *ocaller, unsigned long eventId, void *)
    {
    vtkRenderWindow* renWin = vtkRenderWindow::SafeDownCast(ocaller);
    if (this->Target && this->Target->GetEnabled())
      {
      switch (eventId)
        {
      case vtkCommand::StartEvent:
        this->Target->HandleStartRender(renWin);
        break;

      case vtkCommand::EndEvent:
        this->Target->HandleEndRender(renWin);
        break;

      case vtkCommand::AbortCheckEvent:
        this->Target->HandleAbortRender(renWin);
        break;
        }
      }
    }

  vtkPVSynchronizedRenderWindows* Target;
};

//----------------------------------------------------------------------------
namespace
{
  void RenderRMI(void *localArg,
    void *remoteArg, int remoteArgLength, int vtkNotUsed(remoteProcessId))
    {
    vtkMultiProcessStream stream;
    stream.SetRawData(reinterpret_cast<unsigned char*>(remoteArg),
      remoteArgLength);
    unsigned int id = 0;
    stream >> id;
    vtkPVSynchronizedRenderWindows* self =
      reinterpret_cast<vtkPVSynchronizedRenderWindows*>(localArg);
    self->Render(id);
    }
};

vtkStandardNewMacro(vtkPVSynchronizedRenderWindows);
vtkCxxRevisionMacro(vtkPVSynchronizedRenderWindows, "$Revision$");
//----------------------------------------------------------------------------
vtkPVSynchronizedRenderWindows::vtkPVSynchronizedRenderWindows()
{
  this->Mode = INVALID;
  this->ClientServerController = 0;
  this->ParallelController = 0;
  this->ClientServerRMITag = 0;
  this->ParallelRMITag = 0;
  this->Internals = new vtkInternals();
  this->Observer = vtkObserver::New();
  this->Observer->Target = this;
  this->Enabled = true;
  this->RenderEventPropagation = true;
  this->RenderOneViewAtATime = false;

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  if (!pm)
    {
    vtkErrorMacro(
      "vtkPVSynchronizedRenderWindows cannot be used in the current\n"
      "setup. Aborting for debugging purposes.");
    abort();
    }

  if (pm->GetActiveRemoteConnection() == NULL)
    {
    this->Mode = BUILTIN;
    if (pm->GetNumberOfLocalPartitions() > 1)
      {
      this->Mode = BATCH;
      }
    // It's possible that is this is a satellite node on render-server or
    // data-server.
    switch (pm->GetOptions()->GetProcessType())
      {
    case vtkPVOptions::PVDATA_SERVER:
      this->Mode = DATA_SERVER;
      break;

    case vtkPVOptions::PVRENDER_SERVER:
    case vtkPVOptions::PVSERVER:
      this->Mode = RENDER_SERVER;
      break;
      }
    }
  else if (pm->GetActiveRemoteConnection()->IsA("vtkClientConnection"))
    {
    this->Mode = RENDER_SERVER;
    if (pm->GetOptions()->GetProcessType() == vtkPVOptions::PVDATA_SERVER)
      {
      this->Mode = DATA_SERVER;
      }
    }
  else if (pm->GetActiveRemoteConnection()->IsA("vtkServerConnection"))
    {
    this->Mode = CLIENT;
    }

  // Setup the controllers for the communication.
  switch (this->Mode)
    {
  case BUILTIN:
  case DATA_SERVER:
    // nothing to do.
    break;

  case BATCH:
    this->SetParallelController(vtkMultiProcessController::GetGlobalController());
    break;

  case RENDER_SERVER:
    this->SetParallelController(vtkMultiProcessController::GetGlobalController());
    // this will be NULL on satellites.
    this->SetClientServerController(pm->GetActiveRenderServerSocketController());
    break;

  case CLIENT:
    this->SetClientServerController(pm->GetActiveRenderServerSocketController());
    break;

  default:
    vtkErrorMacro("Invalid process type.");
    abort();
    }
}

//----------------------------------------------------------------------------
vtkPVSynchronizedRenderWindows::~vtkPVSynchronizedRenderWindows()
{
  this->SetClientServerController(0);
  this->SetParallelController(0);

  delete this->Internals;
  this->Internals = 0;

  this->Observer->Target = NULL;
  this->Observer->Delete();
  this->Observer = NULL;
}

//----------------------------------------------------------------------------
bool vtkPVSynchronizedRenderWindows::GetLocalProcessIsDriver()
{
  switch (this->Mode)
    {
  case BUILTIN:
    return true;

  case CLIENT:
    return true;

  case RENDER_SERVER:
    return false;

  case BATCH:
    if (this->ParallelController &&
      this->ParallelController->GetLocalProcessId() == 0)
      {
      return true;
      }

  case DATA_SERVER:
  default:
    return false;
    }
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::SetClientServerController(
  vtkMultiProcessController* controller)
{
  if (this->ClientServerController == controller)
    {
    return;
    }

  if (this->ClientServerController && this->ClientServerRMITag)
    {
    this->ClientServerController->RemoveRMICallback(this->ClientServerRMITag);
    }

  vtkSetObjectBodyMacro(
    ClientServerController, vtkMultiProcessController, controller);
  this->ClientServerRMITag = 0;

  // Only the RENDER_SERVER processes needs to listen to SYNC_MULTI_RENDER_WINDOW_TAG
  // triggers from the client.
  if (controller && this->Mode == RENDER_SERVER)
    {
    this->ClientServerRMITag =
      controller->AddRMICallback(::RenderRMI, this, SYNC_MULTI_RENDER_WINDOW_TAG);
    }
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::SetParallelController(
  vtkMultiProcessController* controller)
{
  if (this->ParallelController == controller)
    {
    return;
    }

  if (this->ParallelController && this->ParallelRMITag)
    {
    this->ParallelController->RemoveRMICallback(this->ParallelRMITag);
    }

  vtkSetObjectBodyMacro(
    ParallelController, vtkMultiProcessController, controller);
  this->ParallelRMITag = 0;

  // Only satellites listen to the SYNC_MULTI_RENDER_WINDOW_TAG
  // triggers from the root.
  if (controller &&
     (this->Mode == RENDER_SERVER || this->Mode == BATCH) &&
     controller->GetLocalProcessId() > 0)
    {
    this->ParallelRMITag =
      controller->AddRMICallback(::RenderRMI, this, SYNC_MULTI_RENDER_WINDOW_TAG);
    }
}

//----------------------------------------------------------------------------
vtkRenderWindow* vtkPVSynchronizedRenderWindows::NewRenderWindow()
{
  switch (this->Mode)
    {
  case DATA_SERVER:
      {
      // we could very return a dummy window here.
      vtkRenderWindow* window = vtkRenderWindow::New();
      return window;
      }

  case BUILTIN:
  case CLIENT:
      {
      // client always creates new window for each view in the multi layout
      // configuration.
      vtkRenderWindow* window = vtkRenderWindow::New();
      window->DoubleBufferOn();
      window->AlphaBitPlanesOn();
      return window;
      }

  case RENDER_SERVER:
  case BATCH:
    // all views share the same render window.
    if (!this->Internals->SharedRenderWindow)
      {
      vtkRenderWindow* window = vtkRenderWindow::New();
      window->DoubleBufferOn();
      window->AlphaBitPlanesOn();
      // SwapBuffers should be ON only on root node in BATCH mode
      // or when operating in tile-display mode.
      bool swap_buffers = false;
      swap_buffers |= (this->Mode == BATCH &&
        this->ParallelController->GetLocalProcessId() == 0);
      int not_used[2];
      swap_buffers |= this->GetTileDisplayParameters(not_used);
      window->SetSwapBuffers(swap_buffers? 1 : 0);
      window->SetSwapBuffers(1); // for debugging FIXME.
      this->Internals->SharedRenderWindow.TakeReference(window);
      }
    else
      {
      cout << "Using shared render window" << endl;
      }
    this->Internals->SharedRenderWindow->Register(this);
    return this->Internals->SharedRenderWindow;

  case INVALID:
    abort();
    }

  return NULL;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::AddRenderWindow(
  unsigned int id, vtkRenderWindow* renWin)
{
  assert(renWin != NULL && id != 0);

  if (this->Internals->RenderWindows.find(id) !=
    this->Internals->RenderWindows.end() &&
    this->Internals->RenderWindows[id].RenderWindow != NULL)
    {
    vtkErrorMacro("ID for render window already in use: " << id);
    return;
    }

  this->Internals->RenderWindows[id].RenderWindow = renWin;
  if (!renWin->HasObserver(vtkCommand::StartEvent, this->Observer))
    {
    this->Internals->RenderWindows[id].StartRenderTag =
      renWin->AddObserver(vtkCommand::StartEvent, this->Observer);
    }
  if (!renWin->HasObserver(vtkCommand::EndEvent, this->Observer))
    {
    this->Internals->RenderWindows[id].EndRenderTag =
      renWin->AddObserver(vtkCommand::EndEvent, this->Observer);
    }
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::RemoveRenderWindow(unsigned int id)
{
  vtkInternals::RenderWindowsMap::iterator iter =
    this->Internals->RenderWindows.find(id);
  if (iter != this->Internals->RenderWindows.end())
    {
    if (iter->second.StartRenderTag)
      {
      iter->second.RenderWindow->RemoveObserver(iter->second.StartRenderTag);
      }
    if (iter->second.EndRenderTag)
      {
      iter->second.RenderWindow->RemoveObserver(iter->second.EndRenderTag);
      }
    this->Internals->RenderWindows.erase(iter);
    }
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::AddRenderer(unsigned int id,
  vtkRenderer* renderer)
{
  this->Internals->RenderWindows[id].Renderers.push_back(renderer);
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::RemoveAllRenderers(unsigned int id)
{
  vtkInternals::RenderWindowsMap::iterator iter =
    this->Internals->RenderWindows.find(id);
  if (iter != this->Internals->RenderWindows.end())
    {
    iter->second.Renderers.clear();
    }
}

//----------------------------------------------------------------------------
vtkRenderWindow* vtkPVSynchronizedRenderWindows::GetRenderWindow(unsigned int id)
{
  vtkInternals::RenderWindowsMap::iterator iter =
    this->Internals->RenderWindows.find(id);
  if (iter != this->Internals->RenderWindows.end())
    {
    return iter->second.RenderWindow;
    }
  return NULL;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::SetWindowSize(unsigned int id,
  int width, int height)
{
  this->Internals->RenderWindows[id].Size[0] = width;
  this->Internals->RenderWindows[id].Size[1] = height;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::SetWindowPosition(unsigned int id,
  int px, int py)
{
  this->Internals->RenderWindows[id].Position[0] = px;
  this->Internals->RenderWindows[id].Position[1] = py;
}

//----------------------------------------------------------------------------
const int *vtkPVSynchronizedRenderWindows::GetWindowSize(unsigned int id)
{
  vtkInternals::RenderWindowsMap::iterator iter =
    this->Internals->RenderWindows.find(id);
  if (iter != this->Internals->RenderWindows.end())
    {
    return iter->second.Size;
    }
  return NULL;
}

//----------------------------------------------------------------------------
const int *vtkPVSynchronizedRenderWindows::GetWindowPosition(unsigned int id)
{
  vtkInternals::RenderWindowsMap::iterator iter =
    this->Internals->RenderWindows.find(id);
  if (iter != this->Internals->RenderWindows.end())
    {
    return iter->second.Position;
    }
  return NULL;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::Render(unsigned int id)
{
  //cout << "Rendering: " << id << endl;
  vtkInternals::RenderWindowsMap::iterator iter =
    this->Internals->RenderWindows.find(id);
  if (iter == this->Internals->RenderWindows.end())
    {
    return;
    }

  // disable all other renderers.
  vtkRendererCollection* renderers = iter->second.RenderWindow->GetRenderers();
  renderers->InitTraversal();
  while (vtkRenderer* ren = renderers->GetNextItem())
    {
    if (ren->GetErase() != 0)
      {
      ren->DrawOff();
      }
    }

  vtkInternals::VectorOfRenderers::iterator iterRen;
  for (iterRen = iter->second.Renderers.begin();
    iterRen != iter->second.Renderers.end(); ++iterRen)
    {
    iterRen->GetPointer()->DrawOn();
    }

  // FIXME: When root node tries to communicate to the satellites the active
  // id, there's no clean way of determining the active id since on root node
  // the render window is shared among all views. Hence, we have this hack :(.
  this->Internals->ActiveId = id;
  iter->second.RenderWindow->Render();
  this->Internals->ActiveId = 0;
  //cout << "Done Rendering: " << id << endl;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::HandleStartRender(vtkRenderWindow* renWin)
{
  // This method is called when a render window starts rendering. This is called
  // on all processes. The response is different on all the processes based on
  // the mode/configuration.

  switch (this->Mode)
    {
  case CLIENT:
    this->ClientStartRender(renWin);
    break;

  case RENDER_SERVER:
  case BATCH:
    if (this->ParallelController->GetLocalProcessId() == 0)
      {
      // root node.
      this->RootStartRender(renWin);
      }
    else
      {
      this->SatelliteStartRender(renWin);
      }
    break;

  case BUILTIN:
  case DATA_SERVER:
  default:
    return;
    }
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::HandleEndRender(vtkRenderWindow*)
{
  switch (this->Mode)
    {
  case CLIENT:
    this->ClientServerController->Barrier();
    break;

  case RENDER_SERVER:
    if (this->ParallelController->GetLocalProcessId() == 0)
      {
      this->ClientServerController->Barrier();
      }
    break;

  default:
    return;
    }
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::ClientStartRender(vtkRenderWindow* renWin)
{
  // In client-server mode, the client needs to collect the window layouts and
  // then the active window specific parameters.

  this->Internals->ActiveId = this->Internals->GetKey(renWin);
  if (this->RenderEventPropagation)
    {
    // Tell the server-root to start rendering.
    vtkMultiProcessStream stream;
    stream << this->Internals->ActiveId;
    vtkstd::vector<unsigned char> data;
    stream.GetRawData(data);
    this->ClientServerController->TriggerRMIOnAllChildren(
      &data[0], static_cast<int>(data.size()), SYNC_MULTI_RENDER_WINDOW_TAG);
    }
  // when this->RenderEventPropagation, we assume that the server activates the
  // correct render window somehow.

  vtkMultiProcessStream stream;

  // Pass in the information about the layout for all the windows.
  // TODO: This gets called when rendering each render window. However, this
  // information does not necessarily get invalidated that frequently. Can we be
  // smart about it?
  this->SaveWindowAndLayout(renWin, stream);

  this->ClientServerController->Broadcast(stream, 0);

  this->UpdateWindowLayout();

  this->Internals->ActiveId = -1;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::RootStartRender(vtkRenderWindow* renWin)
{
  if (this->ClientServerController)
    {
    // * Get window layout from the server. $CODE_GET_LAYOUT_AND_UPDATE$
    vtkMultiProcessStream stream;
    this->ClientServerController->Broadcast(stream, 1);

    // Load the layout for all the windows from the client.
    this->LoadWindowAndLayout(renWin, stream);
    }

  // * Ensure layout i.e. all renders have correct viewports and hide inactive
  //   renderers.
  this->UpdateWindowLayout();

  if (this->ParallelController->GetNumberOfProcesses() <= 1)
    {
    return;
    }

  if (this->RenderEventPropagation)
    {
    // * Tell the satellites to start rendering.
    vtkMultiProcessStream stream;
    stream << this->Internals->ActiveId;
    vtkstd::vector<unsigned char> data;
    stream.GetRawData(data);
    this->ParallelController->TriggerRMIOnAllChildren(
      &data[0], static_cast<int>(data.size()), SYNC_MULTI_RENDER_WINDOW_TAG);
    }

  // * Send the layout and window params to the satellites.
  vtkMultiProcessStream stream;
  // Pass in the information about the layout for all the windows.
  // TODO: This gets called when rendering each render window. However, this
  // information does not necessarily get invalidated that frequently. Can we be
  // smart about it?
  this->SaveWindowAndLayout(renWin, stream);

  this->ParallelController->Broadcast(stream, 0);
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::SatelliteStartRender(
  vtkRenderWindow* renWin)
{
  // * Get window layout from the server. $CODE_GET_LAYOUT_AND_UPDATE$
  if (this->ParallelController)
    {
    vtkMultiProcessStream stream;
    this->ParallelController->Broadcast(stream, 0);

    // Load the layout for all the windows from the root.
    this->LoadWindowAndLayout(renWin, stream);
    }

  // * Ensure layout i.e. all renders have correct viewports and hide inactive
  //   renderers.
  this->UpdateWindowLayout();
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::SaveWindowAndLayout(
  vtkRenderWindow* window, vtkMultiProcessStream& stream)
{
  int full_size[2] = {0, 0};
  stream << static_cast<unsigned int>(this->Internals->RenderWindows.size());

  vtkInternals::RenderWindowsMap::iterator iter;
  for (iter = this->Internals->RenderWindows.begin();
    iter != this->Internals->RenderWindows.end(); ++iter)
    {
    const int *actual_size = iter->second.Size;
    const int *position = iter->second.Position;

    full_size[0] = full_size[0] > (position[0] + actual_size[0])?
      full_size[0] : position[0] + actual_size[0];
    full_size[1] = full_size[1] > (position[1] + actual_size[1])?
      full_size[1] : position[1] + actual_size[1];

    stream << iter->first << position[0] << position[1]
      << actual_size[0] << actual_size[1];
    }

  // Now push the full size.
  stream << full_size[0] << full_size[1];

  // Now save the window's tile scale and tile-viewport.
  int tileScale[2];
  double tileViewport[4];
  window->GetTileScale(tileScale);
  window->GetTileViewport(tileViewport);
  stream << tileScale[0] << tileScale[1]
         << tileViewport[0] << tileViewport[1] << tileViewport[2]
         << tileViewport[3]
         << window->GetDesiredUpdateRate();
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::LoadWindowAndLayout(
  vtkRenderWindow* window, vtkMultiProcessStream& stream)
{
  unsigned int number_of_windows = 0;
  stream >> number_of_windows;

  if (number_of_windows != static_cast<unsigned int>(
      this->Internals->RenderWindows.size()))
    {
    vtkErrorMacro("Mismatch is render windows on different processes. "
      "Aborting for debugging purposes.");
    abort();
    }

  for (unsigned int cc=0; cc < number_of_windows; cc++)
    {
    int actual_size[2];
    int position[2];
    unsigned int key;
    stream >> key >> position[0] >> position[1]
           >> actual_size[0] >> actual_size[1];

    vtkInternals::RenderWindowsMap::iterator iter =
      this->Internals->RenderWindows.find(key);
    if (iter == this->Internals->RenderWindows.end())
      {
      vtkErrorMacro("Don't know anything about windows with key: " << key);
      continue;
      }

    iter->second.Size[0] = actual_size[0];
    iter->second.Size[1] = actual_size[1];
    iter->second.Position[0] = position[0];
    iter->second.Position[1] = position[1];
    }

  // Now load the full size.
  int full_size[2];
  stream >> full_size[0] >> full_size[1];

  // Now load the window's tile scale and tile-viewport.
  // tile-scale and viewport are overloaded. They are used when rendering large
  // images as well as when rendering in tile-display mode. The code here
  // handles the case when rendering large images.
  int tileScale[2];
  double tileViewport[4];
  double desiredUpdateRate;

  stream >> tileScale[0] >> tileScale[1]
         >> tileViewport[0] >> tileViewport[1] >> tileViewport[2]
         >> tileViewport[3]
         >> desiredUpdateRate;
  window->SetTileScale(tileScale);
  window->SetTileViewport(tileViewport);
  window->SetDesiredUpdateRate(desiredUpdateRate);
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::UpdateWindowLayout()
{
  int full_size[2] = {0, 0};
  vtkInternals::RenderWindowsMap::iterator iter;

  if (this->RenderOneViewAtATime)
    {
    iter = this->Internals->RenderWindows.find(this->Internals->ActiveId);
    if (iter != this->Internals->RenderWindows.end())
      {
      iter->second.RenderWindow->SetSize(iter->second.Size);
      double viewport[4] = {0, 0, 1, 1};
      this->Internals->UpdateViewports(
        iter->second.Renderers, viewport);
      }
    return;
    }

  // Compute full_size.
  for (iter = this->Internals->RenderWindows.begin();
    iter != this->Internals->RenderWindows.end(); ++iter)
    {
    const int *actual_size = iter->second.Size;
    const int *position = iter->second.Position;

    full_size[0] = full_size[0] > (position[0] + actual_size[0])?
      full_size[0] : position[0] + actual_size[0];
    full_size[1] = full_size[1] > (position[1] + actual_size[1])?
      full_size[1] : position[1] + actual_size[1];
    }

  switch (this->Mode)
    {
  case CLIENT:
    for (iter = this->Internals->RenderWindows.begin();
      iter != this->Internals->RenderWindows.end(); ++iter)
      {
      //const int *actual_size = iter->second.Size;
      //const int *position = iter->second.Position;
      // This class only supports full-viewports.
      double viewport[4] = {0, 0, 1, 1};
      this->Internals->UpdateViewports(
        iter->second.Renderers, viewport);
      }
    break;

  case RENDER_SERVER:
  case BATCH:
      {
      // If we are in tile-display mode, we should update the tile-scale
      // and tile-viewport for the render window. That is required for the camera
      // as well as for the annotations to show correctly.
      vtkPVServerInformation* server_info =
        vtkProcessModule::GetProcessModule()->GetServerInformation(NULL);
      int tile_dims[2];
      bool in_tile_display_mode = this->GetTileDisplayParameters(tile_dims); 
      if (in_tile_display_mode)
        {
        // FIXME: handle full-screen case
        this->Internals->SharedRenderWindow->SetSize(400, 400);
        //this->Internals->SharedRenderWindow->SetFullScreen(1);

        // TileScale and TileViewport must be setup on render window correctly
        // so that 2D props show up correctly in tile display mode.
        double tile_viewport[4];
        vtkTilesHelper* helper = vtkTilesHelper::New();
        helper->SetTileDimensions(tile_dims);
        helper->SetTileMullions(server_info->GetTileMullions());
        helper->SetTileWindowSize(this->Internals->SharedRenderWindow->GetActualSize());
        helper->GetNormalizedTileViewport(NULL,
          this->ParallelController->GetLocalProcessId(), tile_viewport);
        helper->Delete();

        this->Internals->SharedRenderWindow->SetTileScale(tile_dims);
        this->Internals->SharedRenderWindow->SetTileViewport(tile_viewport);
        }
      else
        {
        // NOTE: if you want to render a higher resolution image on the server,
        // you can very easily do that simply set that size here. Just ensure
        // that the aspect ratio remains the same.
        this->Internals->SharedRenderWindow->SetSize(full_size);
        //this->Internals->SharedRenderWindow->SetPosition(0, 0);
        }

      // Iterate over all (logical) windows and set the viewport on the
      // renderers to reflect the position and size of the actual window on the
      // client side.
      for (iter = this->Internals->RenderWindows.begin();
        iter != this->Internals->RenderWindows.end(); ++iter)
        {
        const int *actual_size = iter->second.Size;
        const int *position = iter->second.Position;

        // This class only supports full-viewports.
        double viewport[4];
        viewport[0] = position[0]/static_cast<double>(full_size[0]);
        viewport[1] = position[1]/static_cast<double>(full_size[1]);
        viewport[2] = (position[0] + actual_size[0])/
          static_cast<double>(full_size[0]);
        viewport[3] = (position[1] + actual_size[1])/
          static_cast<double>(full_size[1]);

        // As far as window-positions go, (0,0) is top-left corner, while for
        // viewport (0, 0) is bottom-left corner. So we flip the Y viewport.
        viewport[1] = 1.0 - viewport[1];
        viewport[3] = 1.0 - viewport[3];
        double temp = viewport[1];
        viewport[1] = viewport[3];
        viewport[3] = temp;

        // This viewport is the viewport for the renderers treating the all the
        // tiles as one large display.
        //cout << "Current Viewport:" << viewport[0]
        //  << ", " << viewport[1] << ", " << viewport[2]
        //  << ", " << viewport[3] << endl;
        this->Internals->UpdateViewports(
          iter->second.Renderers, viewport);
        }
      }
    break;

  case BUILTIN:
  case DATA_SERVER:
  case INVALID:
    abort();
    }
}

//----------------------------------------------------------------------------
bool vtkPVSynchronizedRenderWindows::GetTileDisplayParameters(int tile_dims[2])
{
  vtkPVServerInformation* server_info =
    vtkProcessModule::GetProcessModule()->GetServerInformation(NULL);
  tile_dims[0] = server_info->GetTileDimensions()[0];
  tile_dims[1] = server_info->GetTileDimensions()[1];
  bool in_tile_display_mode = (tile_dims[0] > 0 || tile_dims[1] > 0);
  tile_dims[0] = (tile_dims[0] == 0)? 1 : tile_dims[0];
  tile_dims[1] = (tile_dims[1] == 0)? 1 : tile_dims[1];
  return in_tile_display_mode;
}

//----------------------------------------------------------------------------
bool vtkPVSynchronizedRenderWindows::SynchronizeSize(unsigned long& size)
{
  // handle trivial case.
  if (this->Mode == BUILTIN || this->Mode == INVALID)
    {
    return true;
    }

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();

  vtkMultiProcessController* parallelController =
    this->GetParallelController();
  vtkMultiProcessController* c_rs_controller =
    this->GetClientServerController();
  vtkMultiProcessController* c_ds_controller = NULL;

  if (this->Mode == CLIENT)
    {
    vtkMultiProcessController* cs = pm->GetActiveSocketController();
    // if both are same, then we are not connected to a separate data server
    if (cs != c_rs_controller)
      {
      c_ds_controller = cs;
      }
    }
  else if (this->Mode == DATA_SERVER)
    {
    c_ds_controller = pm->GetActiveSocketController();
    }

  // The strategy is reduce the value to client then broadcast it out to
  // render-server and data-server.
  if (parallelController)
    {
    unsigned long result = size;
    parallelController->Reduce(&size, &result, 1, vtkCommunicator::SUM_OP, 0);
    size = result;
    }

  // on pvdataserver/pvrenderserver/pvserver/pvbatch, we now have collected the
  // size-sum on root node.
  switch (this->Mode)
    {
  case CLIENT:
      {
      if (c_ds_controller)
        {
        unsigned long other_size;
        c_ds_controller->Receive(&other_size, 1, 1, 41232);
        size += other_size;
        }
      if (c_rs_controller)
        {
        unsigned long other_size;
        c_rs_controller->Receive(&other_size, 1, 1, 41232);
        size += other_size;
        }
      if (c_ds_controller)
        {
        c_ds_controller->Send(&size, 1, 1, 41232);
        }
      if (c_rs_controller)
        {
        c_rs_controller->Send(&size, 1, 1, 41232);
        }
      }
    break;

  case DATA_SERVER:
    // both can't be set on a server process.
    if (c_ds_controller)
      {
      c_ds_controller->Send(&size, 1, 1, 41232);
      c_ds_controller->Receive(&size, 1, 1, 41232);
      }
    break;

  case RENDER_SERVER:
    if (c_rs_controller)
      {
      c_rs_controller->Send(&size, 1, 1, 41232);
      c_rs_controller->Receive(&size, 1, 1, 41232);
      }
    break;

  default:
    assert(c_ds_controller==NULL && c_rs_controller == NULL);
    }

  if (parallelController)
    {
    parallelController->Broadcast(&size, 1, 0);
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkPVSynchronizedRenderWindows::SynchronizeBounds(double bounds[6])
{
  // handle trivial case.
  if (this->Mode == BUILTIN || this->Mode == INVALID)
    {
    return true;
    }

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();

  vtkMultiProcessController* parallelController =
    this->GetParallelController();
  vtkMultiProcessController* c_rs_controller =
    this->GetClientServerController();
  vtkMultiProcessController* c_ds_controller = NULL;

  if (this->Mode == CLIENT)
    {
    vtkMultiProcessController* cs = pm->GetActiveSocketController();
    // if both are same, then we are not connected to a separate data server
    if (cs != c_rs_controller)
      {
      c_ds_controller = cs;
      }
    }
  else if (this->Mode == DATA_SERVER)
    {
    c_ds_controller = pm->GetActiveSocketController();
    }

  // The strategy is reduce the value to client then broadcast it out to
  // render-server and data-server.
  if (parallelController)
    {
    double min_bounds[3] = {bounds[0], bounds[2], bounds[4]};
    double max_bounds[3] = {bounds[1], bounds[3], bounds[5]};
    double min_result[3], max_result[3];
    this->ParallelController->Reduce(min_bounds, min_result, 3,
      vtkCommunicator::MIN_OP, 0);
    this->ParallelController->Reduce(max_bounds, max_result, 3,
      vtkCommunicator::MAX_OP, 0);
    bounds[0] = min_result[0];
    bounds[2] = min_result[1];
    bounds[4] = min_result[2];
    bounds[1] = max_result[0];
    bounds[3] = max_result[1];
    bounds[5] = max_result[2];
    }

  // on pvdataserver/pvrenderserver/pvserver/pvbatch, we now have collected the
  // size-sum on root node.
  switch (this->Mode)
    {
  case CLIENT:
      {
      vtkBoundingBox bbox;
      bbox.AddBounds(bounds);

      if (c_ds_controller)
        {
        c_ds_controller->Receive(bounds, 6, 1, 41232);
        bbox.AddBounds(bounds);
        }
      if (c_rs_controller)
        {
        c_rs_controller->Receive(bounds, 6, 1, 41232);
        bbox.AddBounds(bounds);
        }
      bbox.GetBounds(bounds);
      if (c_ds_controller)
        {
        c_ds_controller->Send(bounds, 6, 1, 41232);
        }
      if (c_rs_controller)
        {
        c_rs_controller->Send(bounds, 6, 1, 41232);
        }
      }
    break;

  case DATA_SERVER:
    // both can't be set on a server process.
    if (c_ds_controller != NULL)
      {
      c_ds_controller->Send(bounds, 6, 1, 41232);
      c_ds_controller->Receive(bounds, 6, 1, 41232);
      }
    break;

  case RENDER_SERVER:
    if (c_rs_controller != NULL)
      {
      c_rs_controller->Send(bounds, 6, 1, 41232);
      c_rs_controller->Receive(bounds, 6, 1, 41232);
      }
    break;

  default:
    assert(c_ds_controller==NULL && c_rs_controller == NULL);
    }

  if (parallelController)
    {
    parallelController->Broadcast(bounds, 6, 0);
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkPVSynchronizedRenderWindows::BroadcastToDataServer(vtkSelection* selection)
{
  // handle trivial case.
  if (this->Mode == BUILTIN || this->Mode == INVALID)
    {
    return true;
    }

 vtkProcessModule* pm = vtkProcessModule::GetProcessModule();

  vtkMultiProcessController* parallelController =
    this->GetParallelController();
  vtkMultiProcessController* c_ds_controller =
    this->GetClientServerController();
  if (this->Mode == CLIENT)
    {
    c_ds_controller = pm->GetActiveSocketController();
    }
  else if (this->Mode == RENDER_SERVER)
    {
    // FIXME: we need to ascertain that this a RENDER_SERVER processes and not
    // a pvserver process. if it's a render-server processes, simply return.
    }
  else if (this->Mode == DATA_SERVER)
    {
    c_ds_controller = pm->GetActiveSocketController();
    }
  else if (this->Mode == BATCH &&
    parallelController->GetNumberOfProcesses() <= 1)
    {
    return true;
    }

  vtksys_ios::ostringstream xml_stream;
  vtkSelectionSerializer::PrintXML(xml_stream, vtkIndent(), 1, selection);
  vtkMultiProcessStream stream;
  stream << xml_stream.str();

  if (this->Mode == CLIENT && c_ds_controller)
    {
    c_ds_controller->Send(stream, 1, 41233);
    return true;
    }
  else if (c_ds_controller)
    {
    c_ds_controller->Receive(stream, 1, 41233);
    }

  if (parallelController && parallelController->GetNumberOfProcesses() > 1)
    {
    parallelController->Broadcast(stream, 0);
    }

  vtkstd::string xml;
  stream >> xml;
  vtkSelectionSerializer::Parse(xml.c_str(), selection);
  return true;
}

//----------------------------------------------------------------------------
void vtkPVSynchronizedRenderWindows::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

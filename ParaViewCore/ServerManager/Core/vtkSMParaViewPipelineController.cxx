/*=========================================================================

  Program:   ParaView
  Module:    vtkSMParaViewPipelineController.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMParaViewPipelineController.h"

#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyIterator.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"

#include <cassert>

vtkStandardNewMacro(vtkSMParaViewPipelineController);
//----------------------------------------------------------------------------
vtkSMParaViewPipelineController::vtkSMParaViewPipelineController()
{
}

//----------------------------------------------------------------------------
vtkSMParaViewPipelineController::~vtkSMParaViewPipelineController()
{
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::FindProxy(
  vtkSMSessionProxyManager* pxm, const char* reggroup, const char* xmlgroup, const char* xmltype)
{
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(pxm);
  iter->SetModeToOneGroup();

  for (iter->Begin(reggroup); !iter->IsAtEnd(); iter->Next())
    {
    vtkSMProxy* proxy = iter->GetProxy();
    if (proxy != NULL &&
      proxy->GetXMLGroup() &&
      proxy->GetXMLName() &&
      strcmp(proxy->GetXMLGroup(), xmlgroup) == 0 &&
      strcmp(proxy->GetXMLName(), xmltype) == 0)
      {
      return proxy;
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::InitializeSession(vtkSMSession* session)
{
  assert(session != NULL);

  vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();
  assert(pxm);

  //---------------------------------------------------------------------------
  // If the session is a collaborative session, we need to fetch the state from
  // server before we start creating "essential" proxies. This is a no-op if no
  // a collaborative session.
  pxm->UpdateFromRemote();

  //---------------------------------------------------------------------------
  // Setup selection models used to track active view/active proxy.
  vtkSMProxySelectionModel* selmodel = pxm->GetSelectionModel("ActiveSources");
  if (selmodel == NULL)
    {
    selmodel = vtkSMProxySelectionModel::New();
    pxm->RegisterSelectionModel("ActiveSources", selmodel);
    selmodel->FastDelete();
    }

  selmodel = pxm->GetSelectionModel("ActiveView");
  if (selmodel == NULL)
    {
    selmodel = vtkSMProxySelectionModel::New();
    pxm->RegisterSelectionModel("ActiveView", selmodel);
    selmodel->FastDelete();
    }
  
  //---------------------------------------------------------------------------
  // Create the timekeeper if none exists.
  vtkSmartPointer<vtkSMProxy> timeKeeper = this->FindTimeKeeper(session);
  if (!timeKeeper)
    {
    timeKeeper.TakeReference(pxm->NewProxy("misc", "TimeKeeper"));
    if (!timeKeeper)
      {
      vtkErrorMacro("Failed to create 'TimeKeeper' proxy. ");
      return false;
      }
    timeKeeper->ResetPropertiesToDefault();
    timeKeeper->UpdateVTKObjects();

    pxm->RegisterProxy("timekeeper", timeKeeper);
    }

  //---------------------------------------------------------------------------
  // Create the animation-scene (optional)
  vtkSMProxy* animationScene = this->GetAnimationScene(session);
  if (animationScene)
    {
    // create time-animation track (optional)
    this->GetTimeAnimationTrack(animationScene);
    }

  //---------------------------------------------------------------------------
  // Setup global settings/state for the visualization state.

  // Create the GlobalMapperPropertiesProxy (optional)
  // FIXME: these probably should not be created in collaboration mode on
  // non-master nodes.
  vtkSMProxy* proxy = pxm->NewProxy("misc", "GlobalMapperProperties");
  if (proxy)
    {
    proxy->ResetPropertiesToDefault();
    proxy->UpdateVTKObjects();
    proxy->Delete();
    }

  // Create Strict Load Balancing Proxy
  proxy = pxm->NewProxy("misc", "StrictLoadBalancing");
  if (proxy)
    {
    proxy->ResetPropertiesToDefault();
    proxy->UpdateVTKObjects();
    proxy->Delete();
    }

  return true;
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::FindTimeKeeper(vtkSMSession* session)
{
  assert(session != NULL);

  vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();
  assert(pxm);

  return this->FindProxy(pxm, "timekeeper", "misc", "TimeKeeper");
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::FindAnimationScene(vtkSMSession* session)
{
  assert(session != NULL);

  vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();
  assert(pxm);

  return this->FindProxy(pxm, "animation", "animation", "AnimationScene");
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::GetAnimationScene(vtkSMSession* session)
{
  assert(session != NULL);

  vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();
  assert(pxm);

  vtkSMProxy* timeKeeper = this->FindTimeKeeper(session);
  if (!timeKeeper)
    {
    return NULL;
    }

  vtkSmartPointer<vtkSMProxy> animationScene = this->FindAnimationScene(session);
  if (!animationScene)
    {
    animationScene.TakeReference(pxm->NewProxy("animation", "AnimationScene"));
    if (animationScene)
      {
      vtkSMPropertyHelper(animationScene, "TimeKeeper").Set(timeKeeper);
      animationScene->UpdateVTKObjects();
      pxm->RegisterProxy("animation", animationScene);
      }
    }
  return animationScene.GetPointer();
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::FindTimeAnimationTrack(vtkSMProxy* scene)
{
  if (!scene)
    {
    return NULL;
    }

  vtkSMProxy* timeKeeper = this->FindTimeKeeper(scene->GetSession());
  if (!timeKeeper)
    {
    return NULL;
    }

  vtkSMSessionProxyManager* pxm = scene->GetSessionProxyManager();
  assert(pxm);

  vtkSMPropertyHelper helper(scene, "Cues", /*quiet*/ true);
  for (unsigned int cc=0, max=helper.GetNumberOfElements(); cc < max; cc++)
    {
    vtkSMProxy* cue = helper.GetAsProxy(cc);
    if (cue && cue->GetXMLName() &&
      strcmp(cue->GetXMLName(), "TimeAnimationCue") == 0 &&
      vtkSMPropertyHelper(cue, "AnimatedProxy").GetAsProxy() == timeKeeper)
      {
      vtkSMPropertyHelper pnameHelper(cue, "AnimatedPropertyName");
      if (pnameHelper.GetAsString(0) &&
        strcmp(pnameHelper.GetAsString(0), "Time") == 0)
        {
        return cue;
        }
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::GetTimeAnimationTrack(vtkSMProxy* scene)
{
  vtkSmartPointer<vtkSMProxy> cue = this->FindTimeAnimationTrack(scene);
  if (cue || !scene)
    {
    return cue;
    }

  vtkSMProxy* timeKeeper = this->FindTimeKeeper(scene->GetSession());
  if (!timeKeeper)
    {
    return NULL;
    }

  vtkSMSessionProxyManager* pxm = scene->GetSessionProxyManager();
  assert(pxm);

  cue.TakeReference(pxm->NewProxy("animation", "TimeAnimationCue"));
  if (!cue)
    {
    return NULL;
    }

  cue->ResetPropertiesToDefault();
  vtkSMPropertyHelper(cue, "AnimatedProxy").Set(timeKeeper);
  vtkSMPropertyHelper(cue, "AnimatedPropertyName").Set("Time");
  cue->UpdateVTKObjects();
  pxm->RegisterProxy("animation", cue);

  vtkSMPropertyHelper(scene, "Cues").Add(cue);
  scene->UpdateVTKObjects();

  return cue;
}

//----------------------------------------------------------------------------
void vtkSMParaViewPipelineController::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

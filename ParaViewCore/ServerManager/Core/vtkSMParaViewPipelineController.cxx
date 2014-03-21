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

#include "vtkCommand.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxyIterator.h"
#include "vtkSMProxyListDomain.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSettings.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPointer.h"
#include "vtkWeakPointer.h"

#include <cassert>
#include <map>
#include <set>
#include <vector>
#include <vtksys/ios/sstream>

class vtkSMParaViewPipelineController::vtkInternals
{
public:
  typedef std::map<void*, vtkTimeStamp> TimeStampsMap;
  TimeStampsMap InitializationTimeStamps;
};

namespace
{
  // Used to monitor properties whose domains change.
  class vtkDomainObserver
    {
    std::vector<std::pair<vtkSMProperty*, unsigned long> > MonitoredProperties;
    std::set<vtkSMProperty*> PropertiesWithModifiedDomains;

    void DomainModified(vtkObject* sender, unsigned long, void*)
      {
      vtkSMProperty* prop = vtkSMProperty::SafeDownCast(sender);
      if (prop)
        {
        this->PropertiesWithModifiedDomains.insert(prop);
        }
      }

  public:
    vtkDomainObserver()
      {
      }
    ~vtkDomainObserver()
      {
      for (size_t cc=0; cc < this->MonitoredProperties.size(); cc++)
        {
        this->MonitoredProperties[cc].first->RemoveObserver(
          this->MonitoredProperties[cc].second);
        }
      }
    void Monitor(vtkSMProperty* prop)
      {
      assert(prop != NULL);
      unsigned long oid = prop->AddObserver(vtkCommand::DomainModifiedEvent,
        this, &vtkDomainObserver::DomainModified);
      this->MonitoredProperties.push_back(
        std::pair<vtkSMProperty*, unsigned long>(prop, oid));
      }

    const std::set<vtkSMProperty*>& GetPropertiesWithModifiedDomains() const
      {
      return this->PropertiesWithModifiedDomains;
      }
    };
}

vtkObjectFactoryNewMacro(vtkSMParaViewPipelineController);
//----------------------------------------------------------------------------
vtkSMParaViewPipelineController::vtkSMParaViewPipelineController()
  : Internals(new vtkSMParaViewPipelineController::vtkInternals())
{
}

//----------------------------------------------------------------------------
vtkSMParaViewPipelineController::~vtkSMParaViewPipelineController()
{
  delete this->Internals;
  this->Internals = NULL;
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
bool vtkSMParaViewPipelineController::CreateProxiesForProxyListDomains(
  vtkSMProxy* proxy)
{
  assert(proxy != NULL);
  vtkSmartPointer<vtkSMPropertyIterator> iter;
  iter.TakeReference(proxy->NewPropertyIterator());
  for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
    vtkSMProxyListDomain* pld = iter->GetProperty()?
      vtkSMProxyListDomain::SafeDownCast(iter->GetProperty()->FindDomain("vtkSMProxyListDomain"))
      : NULL;
    if (pld)
      {
      pld->CreateProxies(proxy->GetSessionProxyManager());
      for (unsigned int cc=0, max=pld->GetNumberOfProxies(); cc < max; cc++)
        {
        this->PreInitializeProxy(pld->GetProxy(cc));
        }
      // this is unnecessary here. only done for CompoundSourceProxy instances.
      // those proxies, we generally skip calling "reset" on in
      // PostInitializeProxyInternal(). However, for properties that have proxy
      // list domains, we need to reset them (e.g ProbeLine filter).
      iter->GetProperty()->ResetToDomainDefaults();
      }
    }
  return true;
}

//----------------------------------------------------------------------------
void vtkSMParaViewPipelineController::RegisterProxiesForProxyListDomains(
  vtkSMProxy* proxy)
{
  assert(proxy != NULL);
  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();

  vtksys_ios::ostringstream groupnamestr;
  groupnamestr << "pq_helper_proxies." << proxy->GetGlobalIDAsString();
  std::string groupname = groupnamestr.str();

  vtkSmartPointer<vtkSMPropertyIterator> iter;
  iter.TakeReference(proxy->NewPropertyIterator());
  for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
    vtkSMProxyListDomain* pld = iter->GetProperty()?
      vtkSMProxyListDomain::SafeDownCast(iter->GetProperty()->FindDomain("vtkSMProxyListDomain"))
      : NULL;
    if (!pld)
      {
      continue;
      }
    for (unsigned int cc=0, max=pld->GetNumberOfProxies(); cc < max; cc++)
      {
      vtkSMProxy* plproxy = pld->GetProxy(cc);

      // Handle proxy-list hinks.
      this->ProcessProxyListProxyHints(proxy, plproxy);

      this->PostInitializeProxy(plproxy);
      pxm->RegisterProxy(groupname.c_str(), iter->GetKey(), plproxy);
      }
    }
}

namespace
{
  //---------------------------------------------------------------------------
  class vtkSMLinkObserver : public vtkCommand
  {
public:
  vtkWeakPointer<vtkSMProperty> Output;
  typedef vtkCommand Superclass;
  virtual const char* GetClassNameInternal() const
    { return "vtkSMLinkObserver"; }
  static vtkSMLinkObserver* New()
    {
    return new vtkSMLinkObserver();
    }
  virtual void Execute(vtkObject* caller, unsigned long event, void* calldata)
    {
    (void)event;
    (void)calldata;
    vtkSMProperty* input = vtkSMProperty::SafeDownCast(caller);
    if (input && this->Output)
      {
      // this will copy both checked and unchecked property values.
      this->Output->Copy(input);
      }
    }

  static void LinkProperty(vtkSMProperty* input,
    vtkSMProperty* output)
    {
    if (input && output)
      {
      vtkSMLinkObserver* observer = vtkSMLinkObserver::New();
      observer->Output = output;
      input->AddObserver(vtkCommand::PropertyModifiedEvent, observer);
      input->AddObserver(vtkCommand::UncheckedPropertyModifiedEvent, observer);
      observer->FastDelete();
      output->Copy(input);
      }
    }
  };

}

//----------------------------------------------------------------------------
void vtkSMParaViewPipelineController::ProcessProxyListProxyHints(
  vtkSMProxy* parent, vtkSMProxy* plproxy)
{
  vtkPVXMLElement* proxyListElement = plproxy->GetHints()?
    plproxy->GetHints()->FindNestedElementByName("ProxyList") : NULL;
  if (!proxyListElement)
    {
    return;
    }
  for (unsigned int cc=0, max = proxyListElement->GetNumberOfNestedElements();
    cc < max; ++cc)
    {
    vtkPVXMLElement* child = proxyListElement->GetNestedElement(cc);
    if (child && child->GetName() && strcmp(child->GetName(), "Link") == 0)
      {
      const char* name = child->GetAttribute("name");
      const char* linked_with = child->GetAttribute("with_property");
      if (name && linked_with)
        {
        vtkSMLinkObserver::LinkProperty(
          parent->GetProperty(linked_with), plproxy->GetProperty(name));
        }
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::CreateAnimationHelpers(vtkSMProxy* proxy)
{
  vtkSMSourceProxy* source = vtkSMSourceProxy::SafeDownCast(proxy);
  if (!source)
    {
    return false;
    }
  assert(proxy != NULL);
  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();

  vtksys_ios::ostringstream groupnamestr;
  groupnamestr << "pq_helper_proxies." << proxy->GetGlobalIDAsString();
  std::string groupname = groupnamestr.str();

  for (unsigned int cc=0, max=source->GetNumberOfOutputPorts(); cc < max; cc++)
    {
    vtkSmartPointer<vtkSMProxy> helper;
    helper.TakeReference(pxm->NewProxy("misc", "RepresentationAnimationHelper"));
    if (helper) // since this is optional
      {
      this->PreInitializeProxy(helper);
      vtkSMPropertyHelper(helper, "Source").Set(proxy);
      this->PostInitializeProxy(helper);
      helper->UpdateVTKObjects();

      // yup, all are registered under same name.
      pxm->RegisterProxy(
        groupname.c_str(), "RepresentationAnimationHelper", helper);
      }
    }

  return true;
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
    this->InitializeProxy(timeKeeper);
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
    this->InitializeProxy(proxy);
    proxy->UpdateVTKObjects();
    proxy->Delete();
    }

  // Create Strict Load Balancing Proxy
  proxy = pxm->NewProxy("misc", "StrictLoadBalancing");
  if (proxy)
    {
    this->InitializeProxy(proxy);
    proxy->UpdateVTKObjects();
    proxy->Delete();
    }

  // Animation properties.
  proxy = pxm->NewProxy("misc", "GlobalAnimationProperties");
  if (proxy)
    {
    this->InitializeProxy(proxy);
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
      this->PreInitializeProxy(animationScene);
      vtkSMPropertyHelper(animationScene, "TimeKeeper").Set(timeKeeper);
      this->PostInitializeProxy(animationScene);
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

  this->PreInitializeProxy(cue);
  vtkSMPropertyHelper(cue, "AnimatedProxy").Set(timeKeeper);
  vtkSMPropertyHelper(cue, "AnimatedPropertyName").Set("Time");
  this->PostInitializeProxy(cue);
  cue->UpdateVTKObjects();
  pxm->RegisterProxy("animation", cue);

  vtkSMPropertyHelper(scene, "Cues").Add(cue);
  scene->UpdateVTKObjects();

  return cue;
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMParaViewPipelineController::CreateView(
    vtkSMSession* session, const char* xmlgroup, const char* xmltype)
{
  if (!session)
    {
    return NULL;
    }

  vtkSmartPointer<vtkSMProxy> view;
  view.TakeReference(session->GetSessionProxyManager()->NewProxy(xmlgroup, xmltype));
  return this->InitializeView(view)? view : NULL;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PreInitializeRepresentation(
  vtkSMProxy* proxy)
{
  return proxy? this->PreInitializeProxyInternal(proxy) : false;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PostInitializeRepresentation(vtkSMProxy* proxy)
{
  if (!proxy)
    {
    return false;
    }

  vtkInternals::TimeStampsMap::iterator titer =
    this->Internals->InitializationTimeStamps.find(proxy);
  if (titer == this->Internals->InitializationTimeStamps.end())
    {
    vtkErrorMacro(
      "PostInitializePipelineProxy must only be called after "
      "PreInitializePipelineProxy");
    return false;
    }

  this->PostInitializeProxyInternal(proxy);

  // Now register the proxy itself.
  proxy->GetSessionProxyManager()->RegisterProxy("representations", proxy);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::FinalizeRepresentation(vtkSMProxy* proxy)
{
  if (!proxy)
    {
    return false;
    }
  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();
  std::string groupname("representations");
  const char* _proxyname = pxm->GetProxyName(groupname.c_str(), proxy);
  if (_proxyname == NULL)
    {
    groupname = "scalar_bars";
    _proxyname = pxm->GetProxyName(groupname.c_str(), proxy);
    if (_proxyname == NULL)
      {
      return false;
      }
    }
  const std::string proxyname(_proxyname);

  //---------------------------------------------------------------------------
  // remove the representation from any views.
  typedef std::vector<std::pair<vtkSMProxy*, vtkSMProperty*> > viewsvector;
  viewsvector views;
  for (unsigned int cc=0, max=proxy->GetNumberOfConsumers(); cc < max; cc++)
    {
    vtkSMProxy* consumer = proxy->GetConsumerProxy(cc);
    while (consumer && consumer->GetParentProxy())
      {
      consumer = consumer->GetParentProxy();
      }
    if (consumer && consumer->IsA("vtkSMViewProxy") && proxy->GetConsumerProperty(cc))
      {
      views.push_back(
        std::pair<vtkSMProxy*, vtkSMProperty*>(
          consumer, proxy->GetConsumerProperty(cc)));
      }
    }
  for (viewsvector::iterator iter=views.begin(), max=views.end(); iter != max; ++iter)
    {
    if (iter->first && iter->second)
      {
      vtkSMPropertyHelper(iter->second).Remove(proxy);
      iter->first->UpdateVTKObjects();
      }
    }

  this->FinalizeProxy(proxy);
  proxy->GetSessionProxyManager()->UnRegisterProxy(
    groupname.c_str(), proxyname.c_str(), proxy);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::AddRepresentationToView(
  vtkSMProxy* view, vtkSMProxy* repr)
{
  if (!view || !repr)
    {
    return false;
    }

  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(view->GetProperty("Representations"));
  if (!pp)
    {
    vtkErrorMacro("Failed to locate \"Representations\" property on the view");
    return false;
    }
  if (!pp->IsProxyAdded(repr))
    {
    pp->AddProxy(repr);
    view->UpdateVTKObjects();
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::RemoveRepresentationFromView(vtkSMProxy* view, vtkSMProxy* repr)
{
  if (!view || !repr)
    {
    return false;
    }

  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(view->GetProperty("Representations"));
  if (!pp)
    {
    vtkErrorMacro("Failed to locate \"Representations\" property on the view");
    return false;
    }
  if (pp->IsProxyAdded(repr))
    {
    pp->RemoveProxy(repr);
    view->UpdateVTKObjects();
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::Show(vtkSMProxy* view, vtkSMProxy* repr)
{
  if (this->AddRepresentationToView(view, repr))
    {
    vtkSMPropertyHelper(repr, "Visibility").Set(1);
    repr->UpdateVTKObjects();
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::Hide(vtkSMProxy* view, vtkSMProxy* repr)
{
  if (!repr)
    {
    return false;
    }

  (void)view;
  vtkSMPropertyHelper(repr, "Visibility").Set(0);
  repr->UpdateVTKObjects();
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PreInitializePipelineProxy(vtkSMProxy* proxy)
{
  return proxy? this->PreInitializeProxyInternal(proxy) : false;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PostInitializePipelineProxy(vtkSMProxy* proxy)
{
  if (!proxy)
    {
    return false;
    }

  vtkInternals::TimeStampsMap::iterator titer =
    this->Internals->InitializationTimeStamps.find(proxy);
  if (titer == this->Internals->InitializationTimeStamps.end())
    {
    vtkErrorMacro(
      "PostInitializePipelineProxy must only be called after "
      "PreInitializePipelineProxy");
    return false;
    }

  this->PostInitializeProxyInternal(proxy);

  // Create animation helpers for this proxy.
  this->CreateAnimationHelpers(proxy);

  // Now register the proxy itself.
  proxy->GetSessionProxyManager()->RegisterProxy("sources", proxy);

  // Register proxy with TimeKeeper.
  vtkSMProxy* timeKeeper = this->FindTimeKeeper(proxy->GetSession());
  vtkSMPropertyHelper(timeKeeper, "TimeSources").Add(proxy);
  timeKeeper->UpdateVTKObjects();

  // Make the proxy active.
  vtkSMProxySelectionModel* selmodel =
    proxy->GetSessionProxyManager()->GetSelectionModel("ActiveSources");
  assert(selmodel != NULL);
  selmodel->SetCurrentProxy(proxy, vtkSMProxySelectionModel::CLEAR_AND_SELECT);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::FinalizePipelineProxy(vtkSMProxy* proxy)
{
  if (!proxy)
    {
    return false;
    }
  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();
  const char* _proxyname = pxm->GetProxyName("sources", proxy);
  if (_proxyname == NULL)
    {
    return false;
    }
  const std::string proxyname(_proxyname);

  // ensure proxy is no longer active.
  vtkSMProxySelectionModel* selmodel = pxm->GetSelectionModel("ActiveSources");
  assert(selmodel != NULL);
  if (selmodel->GetCurrentProxy() == proxy)
    {
    selmodel->SetCurrentProxy(NULL, vtkSMProxySelectionModel::CLEAR_AND_SELECT);
    }

  // remove proxy from TimeKeeper.
  vtkSMProxy* timeKeeper = this->FindTimeKeeper(proxy->GetSession());
  vtkSMPropertyHelper(timeKeeper, "TimeSources").Remove(proxy);
  timeKeeper->UpdateVTKObjects();

  // this will remove both proxy-list-domain helpers and animation helpers.
  this->FinalizeProxy(proxy);

  pxm->UnRegisterProxy("sources", proxyname.c_str(), proxy);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::InitializeView(vtkSMProxy* proxy)
{
  if (!this->InitializeProxy(proxy))
    {
    return false;
    }

  // Now register the proxy itself.
  proxy->GetSessionProxyManager()->RegisterProxy("views", proxy);

  // Register proxy with TimeKeeper.
  vtkSMProxy* timeKeeper = this->FindTimeKeeper(proxy->GetSession());
  vtkSMPropertyHelper(timeKeeper, "Views").Add(proxy);
  timeKeeper->UpdateVTKObjects();

  // Register proxy with AnimationScene (optional)
  vtkSMProxy* scene = this->GetAnimationScene(proxy->GetSession());
  if (scene)
    {
    vtkSMPropertyHelper(scene, "ViewModules").Add(proxy);
    scene->UpdateVTKObjects();
    }

  // Make the proxy active.
  vtkSMProxySelectionModel* selmodel =
    proxy->GetSessionProxyManager()->GetSelectionModel("ActiveView");
  assert(selmodel != NULL);
  selmodel->SetCurrentProxy(proxy, vtkSMProxySelectionModel::CLEAR_AND_SELECT);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::FinalizeView(vtkSMProxy* proxy)
{
  if (!proxy)
    {
    return false;
    }
  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();
  const char* _proxyname = pxm->GetProxyName("views", proxy);
  if (_proxyname == NULL)
    {
    return false;
    }
  const std::string proxyname(_proxyname);

  // ensure proxy is no longer active.
  vtkSMProxySelectionModel* selmodel = pxm->GetSelectionModel("ActiveView");
  assert(selmodel != NULL);
  if (selmodel->GetCurrentProxy() == proxy)
    {
    selmodel->SetCurrentProxy(NULL, vtkSMProxySelectionModel::CLEAR_AND_SELECT);
    }

  // remove proxy from AnimationScene (optional)
  vtkSMProxy* scene = this->GetAnimationScene(proxy->GetSession());
  if (scene)
    {
    vtkSMPropertyHelper(scene, "ViewModules").Remove(proxy);
    scene->UpdateVTKObjects();
    }

  // remove proxy from TimeKeeper.
  vtkSMProxy* timeKeeper = this->FindTimeKeeper(proxy->GetSession());
  vtkSMPropertyHelper(timeKeeper, "Views").Remove(proxy);
  timeKeeper->UpdateVTKObjects();

  // remove all representation proxies.
  const char* pnames[]={"Representations", "HiddenRepresentations",
    "Props", "HiddenProps", NULL};
  for (int index=0; pnames[index] != NULL; ++index)
    {
    vtkSMProperty* prop = proxy->GetProperty(pnames[index]);
    if (prop == NULL)
      {
      continue;
      }

    typedef std::vector<vtkWeakPointer<vtkSMProxy> > proxyvectortype;
    proxyvectortype reprs;

    vtkSMPropertyHelper helper(prop);
    for (unsigned int cc=0, max=helper.GetNumberOfElements(); cc < max ; ++cc)
      {
      reprs.push_back(helper.GetAsProxy(cc));
      }

    for (proxyvectortype::iterator iter=reprs.begin(), end=reprs.end(); iter!=end; ++iter)
      {
      if (iter->GetPointer())
        {
        this->Finalize(iter->GetPointer());
        }
      }
    }

  // this will remove both proxy-list-domain helpers and animation helpers.
  this->FinalizeProxy(proxy);

  pxm->UnRegisterProxy("views", proxyname.c_str(), proxy);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PreInitializeProxy(vtkSMProxy* proxy)
{
  return proxy? this->PreInitializeProxyInternal(proxy) : false;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PostInitializeProxy(vtkSMProxy* proxy)
{
  return proxy? this->PostInitializeProxyInternal(proxy) : false;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PreInitializeProxyInternal(vtkSMProxy* proxy)
{
  assert(proxy != NULL);

  // 1. Load XML defaults
  //    (already done by NewProxy() call).

  // 2. Create proxies for proxy-list domains.
  if (!this->CreateProxiesForProxyListDomains(proxy))
    {
    return false;
    }

  // 3. Load property values from Settings
  std::string jsonPrefix(".");
  jsonPrefix.append(proxy->GetXMLGroup());

  vtkSMSettings * settings = vtkSMSettings::GetInstance();
  settings->GetProxySettings(proxy, jsonPrefix.c_str());

  // Now, update the initialization time.
  this->Internals->InitializationTimeStamps[proxy].Modified();

  // Note: we need to be careful with *** vtkSMCompoundSourceProxy ***
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::PostInitializeProxyInternal(vtkSMProxy* proxy)
{
  assert(proxy != NULL);
  vtkInternals::TimeStampsMap::iterator titer =
    this->Internals->InitializationTimeStamps.find(proxy);
  assert(titer != this->Internals->InitializationTimeStamps.end());

  vtkTimeStamp ts = titer->second;
  this->Internals->InitializationTimeStamps.erase(titer);

  // ensure everything is up-to-date.
  proxy->UpdateVTKObjects();

  // FIXME: need to figure out how we should really deal with this.
  // We don't reset properties on custom filter.
  if (!proxy->IsA("vtkSMCompoundSourceProxy"))
    {
    if (vtkSMSourceProxy* sourceProxy = vtkSMSourceProxy::SafeDownCast(proxy))
      {
      // Since domains depend on information properties, it's essential we update
      // property information first.
      sourceProxy->UpdatePipelineInformation();
      }

    // Reset property values using domains. However, this should only be done for
    // properties that were not modified between the PreInitializePipelineProxy and
    // PostInitializePipelineProxy calls.

    vtkSmartPointer<vtkSMPropertyIterator> iter;
    iter.TakeReference(proxy->NewPropertyIterator());

    // iterate over properties and reset them to default. if any property says its
    // domain is modified after we reset it, we need to reset it again since its
    // default may have changed.
    vtkDomainObserver observer;

    for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
      {
      vtkSMProperty* smproperty = iter->GetProperty();

      if ((smproperty->GetMTime() > ts) ||
        !smproperty->GetInformationOnly())
        {
        vtkPVXMLElement* propHints = iter->GetProperty()->GetHints();
        if (propHints && propHints->FindNestedElementByName("NoDefault"))
          {
          // Don't reset properties that request overriding of the default mechanism.
          continue;
          }

        observer.Monitor(iter->GetProperty());
        iter->GetProperty()->ResetToDomainDefaults();
        }
      }

    const std::set<vtkSMProperty*> &props =
      observer.GetPropertiesWithModifiedDomains();
    for (std::set<vtkSMProperty*>::const_iterator iter = props.begin(); iter != props.end(); ++iter)
      {
      (*iter)->ResetToDomainDefaults();
      }
    proxy->UpdateVTKObjects();
    }

  // Register proxies created for proxy list domains.
  this->RegisterProxiesForProxyListDomains(proxy);
  return true;
}
//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::FinalizeProxyInternal(vtkSMProxy* proxy)
{
  assert(proxy != NULL);

  //---------------------------------------------------------------------------
  // Unregister all helper proxies. This will "finalize" all the helper
  // proxies which ensures that any "dependent" proxies for those helpers
  // also get removed.
  vtksys_ios::ostringstream groupnamestr;
  groupnamestr << "pq_helper_proxies." << proxy->GetGlobalIDAsString();
  std::string groupname = groupnamestr.str();

  typedef std::pair<std::string, vtkWeakPointer<vtkSMProxy> > proxymapitemtype;
  typedef std::vector<proxymapitemtype> proxymaptype;
  proxymaptype proxymap;

  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();
    {
    vtkNew<vtkSMProxyIterator> iter;
    iter->SetSessionProxyManager(pxm);
    iter->SetModeToOneGroup();
    for (iter->Begin(groupnamestr.str().c_str()); !iter->IsAtEnd(); iter->Next())
      {
      proxymap.push_back(proxymapitemtype(
          iter->GetKey(), iter->GetProxy()));
      }
    }
  for (proxymaptype::iterator iter = proxymap.begin(), end = proxymap.end();
    iter != end; ++iter)
    {
    if (iter->second)
      {
      this->FinalizeProxy(iter->second);
      pxm->UnRegisterProxy(
        groupnamestr.str().c_str(), iter->first.c_str(), iter->second);
      }
    }

  //---------------------------------------------------------------------------
  // Before going any further build a list of all consumer proxies
  // (not pointing to internal/sub-proxies).

  typedef std::vector<vtkWeakPointer<vtkSMProxy> > proxyvectortype;
  proxyvectortype consumers;

  for (unsigned int cc=0, max = proxy->GetNumberOfConsumers(); cc < max; ++cc)
    {
    vtkSMProxy* consumer = proxy->GetConsumerProxy(cc);
    while (consumer && consumer->GetParentProxy())
      {
      consumer = consumer->GetParentProxy();
      }
    if (consumer)
      {
      consumers.push_back(consumer);
      }
    }

  //---------------------------------------------------------------------------
  // Remove representations connected to this proxy, if any.
  for (proxyvectortype::iterator iter = consumers.begin(), max = consumers.end();
    iter != max; ++iter)
    {
    vtkSMProxy* consumer = iter->GetPointer();
    if (consumer &&
        consumer->IsA("vtkSMRepresentationProxy"))
      {
      this->Finalize(consumer);
      }
    }

  //---------------------------------------------------------------------------
  // TODO: remove any animation cues for this proxy
  // TODO: remove any property links/proxy link setup for this proxy.

  return true;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::FinalizeProxy(vtkSMProxy* proxy)
{
  return proxy? this->FinalizeProxyInternal(proxy) : false;
}

//----------------------------------------------------------------------------
bool vtkSMParaViewPipelineController::Finalize(vtkSMProxy* proxy)
{
  if (!proxy)
    {
    return false;
    }

  // determine what type of proxy is this, based on that, we can finalize it.
  vtkSMSessionProxyManager* pxm = proxy->GetSessionProxyManager();
  if (pxm->GetProxyName("sources", proxy))
    {
    return this->FinalizePipelineProxy(proxy);
    }
  else if (pxm->GetProxyName("representations", proxy) ||
           pxm->GetProxyName("scalar_bars", proxy))
    {
    return this->FinalizeRepresentation(proxy);
    }
  else if (pxm->GetProxyName("views", proxy))
    {
    return this->FinalizeView(proxy);
    }

  return false;
}

//----------------------------------------------------------------------------
void vtkSMParaViewPipelineController::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
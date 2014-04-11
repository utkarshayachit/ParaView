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
#include "vtkSMTimeKeeperProxy.h"

#include "vtkObjectFactory.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTimeKeeper.h"

vtkStandardNewMacro(vtkSMTimeKeeperProxy);
//----------------------------------------------------------------------------
vtkSMTimeKeeperProxy::vtkSMTimeKeeperProxy()
{
}

//----------------------------------------------------------------------------
vtkSMTimeKeeperProxy::~vtkSMTimeKeeperProxy()
{
}

//----------------------------------------------------------------------------
void vtkSMTimeKeeperProxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }
  this->Superclass::CreateVTKObjects();
  if (this->ObjectsCreated)
    {
    vtkSMTimeKeeper* tk = vtkSMTimeKeeper::SafeDownCast(
      this->GetClientSideObject());
    if (tk)
      {
      tk->SetTimestepValuesProperty(this->GetProperty("TimestepValues"));
      tk->SetTimeRangeProperty(this->GetProperty("TimeRange"));
      tk->SetTimeLabelProperty(this->GetProperty("TimeLabel"));
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSMTimeKeeperProxy::AddTimeSource(vtkSMProxy* proxy, bool suppress_input)
{
  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(
    this->GetProperty("TimeSources"));
  if (!proxy || pp->IsProxyAdded(proxy))
    {
    return false;
    }

  // Suppress produces for proxy
  vtkSMProxyProperty* stspp = vtkSMProxyProperty::SafeDownCast(
    this->GetProperty("SuppressedTimeSources"));
  for (unsigned int cc=0, max=proxy->GetNumberOfProducers(); suppress_input && cc < max; cc++)
    {
    vtkSMProxy* producer = proxy->GetProducerProxy(cc);
    producer = producer? producer->GetTrueParentProxy() : NULL;
    if (producer && !stspp->IsProxyAdded(producer))
      {
      stspp->AddProxy(producer);
      }
    }
  // add the requested proxy.
  pp->AddProxy(proxy);
  this->UpdateVTKObjects();
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMTimeKeeperProxy::RemoveTimeSource(vtkSMProxy* proxy, bool unsuppress_input)
{
  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(
    this->GetProperty("TimeSources"));
  if (!proxy || !pp->IsProxyAdded(proxy))
    {
    return false;
    }

  // Remove producer suppressions.
  vtkSMProxyProperty* stspp = vtkSMProxyProperty::SafeDownCast(
    this->GetProperty("SuppressedTimeSources"));
  for (unsigned int cc=0, max=proxy->GetNumberOfProducers(); unsuppress_input && cc < max; cc++)
    {
    vtkSMProxy* producer = proxy->GetProducerProxy(cc);
    producer = producer? vtkSMSourceProxy::SafeDownCast(producer->GetTrueParentProxy()): NULL;
    if (producer)
      {
      stspp->RemoveProxy(producer);
      }
    }

  //remove the requested proxy.
  pp->RemoveProxy(proxy);
  this->UpdateVTKObjects();
  return true;
}

//----------------------------------------------------------------------------
bool vtkSMTimeKeeperProxy::IsTimeSourceTracked(vtkSMProxy* proxy)
{
  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(
    this->GetProperty("TimeSources"));
  vtkSMProxyProperty* stspp = vtkSMProxyProperty::SafeDownCast(
    this->GetProperty("SuppressedTimeSources"));
  return (proxy && pp->IsProxyAdded(proxy) && !stspp->IsProxyAdded(proxy));
}

//----------------------------------------------------------------------------
bool vtkSMTimeKeeperProxy::SetSuppressTimeSource(vtkSMProxy* proxy, bool suppress)
{
  if (proxy)
    {
    vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(
      this->GetProperty("SuppressedTimeSources"));
    if (suppress && pp->IsProxyAdded(proxy) == false)
      {
      pp->AddProxy(proxy);
      }
    else if (!suppress && pp->IsProxyAdded(proxy))
      {
      pp->RemoveProxy(proxy);
      }
    this->UpdateVTKObjects();
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkSMTimeKeeperProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

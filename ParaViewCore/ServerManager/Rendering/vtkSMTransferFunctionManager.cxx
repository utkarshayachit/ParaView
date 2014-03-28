/*=========================================================================

  Program:   ParaView
  Module:    vtkSMTransferFunctionManager.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMTransferFunctionManager.h"

#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyIterator.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMTransferFunctionProxy.h"

#include <assert.h>
#include <vtksys/ios/sstream>
#include <vtksys/RegularExpression.hxx>

namespace
{
  vtkSMProxy* FindProxy(const char* groupName,
    const char* arrayName,
    vtkSMSessionProxyManager* pxm)
    {
    // Proxies are registered as  (arrayName).(ProxyXMLName).
    // In previous versions, they were registered as
    // (numComps).(arrayName).(ProxyXMLName). We dropped the numComps, but this
    // lookup should still match those old LUTs loaded from state.
    vtksys_ios::ostringstream expr;
    expr << "^[0-9.]*" << arrayName << "\\.";
    vtksys::RegularExpression regExp(expr.str().c_str());
    vtkNew<vtkSMProxyIterator> iter;
    iter->SetSessionProxyManager(pxm);
    iter->SetModeToOneGroup();
    for (iter->Begin(groupName); !iter->IsAtEnd(); iter->Next())
      {
      const char* key = iter->GetKey();
      if (key && regExp.find(key))
        {
        assert(iter->GetProxy() != NULL);
        return iter->GetProxy();
        }
      }
    return NULL;
    }
}

vtkStandardNewMacro(vtkSMTransferFunctionManager);
//----------------------------------------------------------------------------
vtkSMTransferFunctionManager::vtkSMTransferFunctionManager()
{
}

//----------------------------------------------------------------------------
vtkSMTransferFunctionManager::~vtkSMTransferFunctionManager()
{
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMTransferFunctionManager::GetColorTransferFunction(
  const char* arrayName, vtkSMSessionProxyManager* pxm)
{
  assert(arrayName != NULL && pxm != NULL);

  vtkSMProxy* proxy = FindProxy("lookup_tables", arrayName, pxm);
  if (proxy)
    {
    return proxy;
    }

  // Create a new one.
  proxy = pxm->NewProxy("lookup_tables", "PVLookupTable");
  if (!proxy)
    {
    vtkErrorMacro("Failed to create PVLookupTable proxy.");
    return NULL;
    }

  vtksys_ios::ostringstream proxyName;
  proxyName << arrayName << ".PVLookupTable";
  if (proxy->GetProperty("ScalarOpacityFunction"))
    {
    vtkSMProxy* sof = this->GetOpacityTransferFunction(arrayName, pxm);
    if (sof)
      {
      sof->UpdateVTKObjects();
      vtkSMPropertyHelper(proxy, "ScalarOpacityFunction").Set(sof);
      }
    }
  pxm->RegisterProxy("lookup_tables", proxyName.str().c_str(), proxy);
  proxy->FastDelete();
  proxy->UpdateVTKObjects();
  return proxy;
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMTransferFunctionManager::GetOpacityTransferFunction(
  const char* arrayName, vtkSMSessionProxyManager* pxm)
{
  assert(arrayName != NULL && pxm != NULL);

  vtkSMProxy* proxy = FindProxy("piecewise_functions", arrayName, pxm);
  if (proxy)
    {
    return proxy;
    }

  // Create a new one.
  proxy = pxm->NewProxy("piecewise_functions", "PiecewiseFunction");
  if (!proxy)
    {
    vtkErrorMacro("Failed to create PVLookupTable proxy.");
    return NULL;
    }

  vtksys_ios::ostringstream proxyName;
  proxyName << arrayName << ".PiecewiseFunction";
  pxm->RegisterProxy("piecewise_functions", proxyName.str().c_str(), proxy);
  proxy->FastDelete();
  proxy->UpdateVTKObjects();
  return proxy;
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMTransferFunctionManager::GetScalarBarRepresentation(
  vtkSMProxy* colorTransferFunction, vtkSMProxy* view)
{
  if (colorTransferFunction == NULL || view == NULL ||
      !colorTransferFunction->IsA("vtkSMTransferFunctionProxy"))
    {
    return NULL;
    }

  vtkSMProxy* scalarBarProxy =
    vtkSMTransferFunctionProxy::FindScalarBarRepresentation(
      colorTransferFunction, view);
  if (scalarBarProxy)
    {
    return scalarBarProxy;
    }

  // Create a new scalar bar representation.
  vtkSMSessionProxyManager* pxm =
    colorTransferFunction->GetSessionProxyManager();
  scalarBarProxy = pxm->NewProxy("representations", "ScalarBarWidgetRepresentation");
  if (!scalarBarProxy)
    {
    vtkErrorMacro("Failed to create ScalarBarWidgetRepresentation proxy.");
    return NULL;
    }

  vtkSMPropertyHelper(scalarBarProxy, "LookupTable").Set(colorTransferFunction);
  scalarBarProxy->ResetPropertiesToDefault();
  scalarBarProxy->UpdateVTKObjects();
 
  pxm->RegisterProxy("scalar_bars",
    pxm->GetUniqueProxyName("scalar_bars", "ScalarBarWidgetRepresentation").c_str(),
    scalarBarProxy);
  scalarBarProxy->FastDelete();

  vtkSMPropertyHelper(view, "Representations").Add(scalarBarProxy);
  view->UpdateVTKObjects();
  return scalarBarProxy;
}

//----------------------------------------------------------------------------
void vtkSMTransferFunctionManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

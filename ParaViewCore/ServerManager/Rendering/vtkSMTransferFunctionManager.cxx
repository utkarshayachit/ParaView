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

#include <assert.h>
#include <vtksys/ios/sstream>
#include <vtksys/RegularExpression.hxx>

namespace
{
  vtkSMProxy* FindProxy(const char* groupName,
    const char* arrayName, int numComponents,
    vtkSMSessionProxyManager* pxm)
    {
    // proxies are registered as (numComps).(arrayName).(ProxyXMLName)
    vtksys_ios::ostringstream expr;
    expr << "^" << numComponents << "\\." << arrayName << "\\.";
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
  const char* arrayName, int numComponents, vtkSMSessionProxyManager* pxm)
{
  assert(arrayName != NULL && numComponents >=0 && pxm != NULL);

  vtkSMProxy* proxy = FindProxy("lookup_tables", arrayName, numComponents, pxm);
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
  proxyName << numComponents << "." << arrayName << ".PVLookupTable";
  if (proxy->GetProperty("ScalarOpacityFunction"))
    {
    vtkSMProxy* sof = this->GetOpacityTransferFunction(arrayName, numComponents, pxm);
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
  const char* arrayName, int numComponents, vtkSMSessionProxyManager* pxm)
{
  assert(arrayName != NULL && numComponents >=0 && pxm != NULL);

  vtkSMProxy* proxy = FindProxy("piecewise_functions", arrayName, numComponents, pxm);
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
  proxyName << numComponents << "." << arrayName << ".PiecewiseFunction";
  pxm->RegisterProxy("piecewise_functions", proxyName.str().c_str(), proxy);
  proxy->FastDelete();
  proxy->UpdateVTKObjects();
  return proxy;
}

//----------------------------------------------------------------------------
void vtkSMTransferFunctionManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

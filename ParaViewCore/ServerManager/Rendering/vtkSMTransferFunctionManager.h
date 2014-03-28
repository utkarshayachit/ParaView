/*=========================================================================

  Program:   ParaView
  Module:    vtkSMTransferFunctionManager.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMTransferFunctionManager - manages transfer functions i.e. color
// lookuptables and opacity piecewise functions for ParaView applications.
// .SECTION Description
// vtkSMTransferFunctionManager manages transfer functions i.e. color
// lookuptables and opacity piecewise functions for ParaView applications.
// vtkSMTransferFunctionManager implements the ParaView specific mechanism for
// managing such transfer function proxies where there's one transfer function
// created and maintained per data array name.
//
// vtkSMTransferFunctionManager has no state. You can create as many instances as
// per your choosing to call the methods. It uses the session proxy manager to
// locate proxies registered using specific names under specific groups. Thus,
// the state is maintained by the proxy manager itself.

#ifndef __vtkSMTransferFunctionManager_h
#define __vtkSMTransferFunctionManager_h

#include "vtkSMObject.h"
#include "vtkPVServerManagerRenderingModule.h" // needed for export macro

class vtkSMProxy;
class vtkSMSessionProxyManager;

class VTKPVSERVERMANAGERRENDERING_EXPORT vtkSMTransferFunctionManager : public vtkSMObject
{
public:
  static vtkSMTransferFunctionManager* New();
  vtkTypeMacro(vtkSMTransferFunctionManager, vtkSMObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Returns a color transfer function proxy instance for mapping a data array
  // with the given name and number-of-components. If none exists in the given
  // session, a new instance will be created and returned.
  vtkSMProxy* GetColorTransferFunction(const char* arrayName, int numComponents,
    vtkSMSessionProxyManager* pxm);

  // Description:
  // Returns a opacity transfer function proxy (aka Piecewise Function) instance
  // for mapping a data array with the given name and number-of-components. If
  // none exists in the given session, a new instance will be created and
  // returned.
  vtkSMProxy* GetOpacityTransferFunction(const char* arrayName, int numComponents,
    vtkSMSessionProxyManager* pxm);

//BTX
protected:
  vtkSMTransferFunctionManager();
  ~vtkSMTransferFunctionManager();

private:
  vtkSMTransferFunctionManager(const vtkSMTransferFunctionManager&); // Not implemented
  void operator=(const vtkSMTransferFunctionManager&); // Not implemented
//ETX
};

#endif

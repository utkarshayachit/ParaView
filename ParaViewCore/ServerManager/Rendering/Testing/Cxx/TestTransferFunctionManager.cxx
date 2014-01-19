/*=========================================================================

Program:   ParaView
Module:    TestTransferFunctionManager.cxx

Copyright (c) Kitware, Inc.
All rights reserved.
See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkInitializationHelper.h"
#include "vtkNew.h"
#include "vtkProcessModule.h"
#include "vtkSMSession.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMTransferFunctionManager.h"

#include <vtksys/ios/sstream>

int main(int argc, char* argv[])
{
  vtkInitializationHelper::Initialize(argv[0], vtkProcessModule::PROCESS_CLIENT);

  // Create a new session.
  vtkSMSession* session = vtkSMSession::New();
  vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();

  vtkNew<vtkSMTransferFunctionManager> mgr;
  vtkSMProxy* colorTF = mgr->GetColorTransferFunction("arrayOne", 3, pxm);
  if (colorTF == NULL)
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }

  // colorTF must match on multiple calls.
  if (colorTF != mgr->GetColorTransferFunction("arrayOne", 3, pxm))
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }

  // colorTF must be different for different arrays.
  if (colorTF == mgr->GetColorTransferFunction("arrayOne", 1, pxm) ||
      colorTF == mgr->GetColorTransferFunction("arrayTwo", 3, pxm))
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }

  vtkSMProxy* opacityTF = mgr->GetOpacityTransferFunction("arrayOne", 3, pxm);
  if (!opacityTF)
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }
  if (opacityTF != mgr->GetOpacityTransferFunction("arrayOne", 3, pxm))
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }
  if (opacityTF == mgr->GetOpacityTransferFunction("arrayOne", 1, pxm) ||
     opacityTF == mgr->GetOpacityTransferFunction("arrayTwo", 3, pxm))
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }

  if (vtkSMPropertyHelper(colorTF, "ScalarOpacityFunction").GetAsProxy() !=
    opacityTF)
    {
    cerr << "ERROR: Failed at line " << __LINE__ << endl;
    return EXIT_FAILURE;
    }
  session->Delete();
  vtkInitializationHelper::Finalize();
  return EXIT_SUCCESS;
}

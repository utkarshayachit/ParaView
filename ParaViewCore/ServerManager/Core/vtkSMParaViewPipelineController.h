/*=========================================================================

  Program:   ParaView
  Module:    vtkSMParaViewPipelineController.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMParaViewPipelineController
// .SECTION Description
//

#ifndef __vtkSMParaViewPipelineController_h
#define __vtkSMParaViewPipelineController_h

#include "vtkSMObject.h"

class vtkSMProxy;
class vtkSMSession;
class vtkSMSessionProxyManager;

class VTKPVSERVERMANAGERCORE_EXPORT vtkSMParaViewPipelineController : public vtkSMObject
{
public:
  static vtkSMParaViewPipelineController* New();
  vtkTypeMacro(vtkSMParaViewPipelineController, vtkSMObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Call this method to setup a branch new session with state considered
  // essential for ParaView session. Returns true on success.
  virtual bool InitializeSession(vtkSMSession* session);

  // Description:
  // Returns the TimeKeeper proxy associated with the session.
  virtual vtkSMProxy* FindTimeKeeper(vtkSMSession* session);

  //---------------------------------------------------------------------------
  // *******  Methods for Pipeline processing objects *********

  // Description:
  // Begin creation of a source/reader/filter proxy.



  //---------------------------------------------------------------------------
  // *******  Methods for Animation   *********

  // Description:
  // Returns the animation scene, if any. Returns NULL if none exists.
  virtual vtkSMProxy* FindAnimationScene(vtkSMSession* session);

  // Description:
  // Returns the animation scene for the session. If none exists, a new one will
  // be created. This may returns NULL if animation scene proxy is not available
  // in the session.
  virtual vtkSMProxy* GetAnimationScene(vtkSMSession* session);
 
  // Description:
  // Return the animation track for time, if any. Returns NULL if none exists.
  virtual vtkSMProxy* FindTimeAnimationTrack(vtkSMProxy* scene);

  // Description:
  // Return the animation track for time. If none exists, a new one will be
  // created. Returns NULL if the proxy is not available in the session.
  virtual vtkSMProxy* GetTimeAnimationTrack(vtkSMProxy* scene);
  //---------------------------------------------------------------------------




//BTX
protected:
  vtkSMParaViewPipelineController();
  ~vtkSMParaViewPipelineController();

  // Description:
  // Find proxy of the group type (xmlgroup, xmltype) registered under a
  // particular group (reggroup). Returns the first proxy found, if any.
  vtkSMProxy* FindProxy(vtkSMSessionProxyManager* pxm,
    const char* reggroup, const char* xmlgroup, const char* xmltype);
 

private:
  vtkSMParaViewPipelineController(const vtkSMParaViewPipelineController&); // Not implemented
  void operator=(const vtkSMParaViewPipelineController&); // Not implemented
//ETX
};

#endif

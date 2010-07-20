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
// .NAME vtkSMRepresentationProxy2
// .SECTION Description
//

#ifndef __vtkSMRepresentationProxy2_h
#define __vtkSMRepresentationProxy2_h

#include "vtkSMSourceProxy.h"

class VTK_EXPORT vtkSMRepresentationProxy2 : public vtkSMSourceProxy
{
public:
  static vtkSMRepresentationProxy2* New();
  vtkTypeMacro(vtkSMRepresentationProxy2, vtkSMSourceProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Calls MarkDirty() and invokes ModifiedEvent.
  virtual void MarkModified(vtkSMProxy* modifiedProxy);

//BTX
protected:
  vtkSMRepresentationProxy2();
  ~vtkSMRepresentationProxy2();

private:
  vtkSMRepresentationProxy2(const vtkSMRepresentationProxy2&); // Not implemented
  void operator=(const vtkSMRepresentationProxy2&); // Not implemented
//ETX
};

#endif

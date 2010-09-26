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
// .NAME vtkSMRepresentationProxy
// .SECTION Description
//

#ifndef __vtkSMRepresentationProxy_h
#define __vtkSMRepresentationProxy_h

#include "vtkSMSourceProxy.h"

class VTK_EXPORT vtkSMRepresentationProxy : public vtkSMSourceProxy
{
public:
  static vtkSMRepresentationProxy* New();
  vtkTypeMacro(vtkSMRepresentationProxy, vtkSMSourceProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Calls MarkDirty() and invokes ModifiedEvent.
  virtual void MarkDirty(vtkSMProxy* modifiedProxy);

  vtkPVDataInformation* GetRepresentedDataInformation(int vtkNotUsed(update))
    { return this->Superclass::GetDataInformation(); }
  vtkPVDataInformation* GetRepresentedDataInformation()
    { return this->Superclass::GetDataInformation(); }

//BTX
protected:
  vtkSMRepresentationProxy();
  ~vtkSMRepresentationProxy();

  virtual void CreateVTKObjects();

  void RepresentationUpdated();

private:
  vtkSMRepresentationProxy(const vtkSMRepresentationProxy&); // Not implemented
  void operator=(const vtkSMRepresentationProxy&); // Not implemented
//ETX
};

#endif

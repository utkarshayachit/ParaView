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
// .NAME vtkPVFetchDataFilter
// .SECTION Description
//

#ifndef __vtkPVFetchDataFilter_h
#define __vtkPVFetchDataFilter_h

#include "vtkClientServerMoveData.h"

class VTK_EXPORT vtkPVFetchDataFilter : public vtkClientServerMoveData
{
public:
  static vtkPVFetchDataFilter* New();
  vtkTypeMacro(vtkPVFetchDataFilter, vtkClientServerMoveData);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set/Get the post gather helper.
  vtkSetStringMacro(PostGatherHelperName);
  vtkGetStringMacro(PostGatherHelperName);

  // Description:
  // Set/Get the pre gather helper.
  vtkSetStringMacro(PreGatherHelperName);
  vtkGetStringMacro(PreGatherHelperName);

//BTX
protected:
  vtkPVFetchDataFilter();
  ~vtkPVFetchDataFilter();

  virtual int RequestData(vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector);


  char* PostGatherHelperName;
  char* PreGatherHelperName;
private:
  vtkPVFetchDataFilter(const vtkPVFetchDataFilter&); // Not implemented
  void operator=(const vtkPVFetchDataFilter&); // Not implemented
//ETX
};

#endif

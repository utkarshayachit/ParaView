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
#include "vtkPVDataRepresentationPipeline.h"

#include "vtkObjectFactory.h"
#include "vtkPVDataRepresentation.h"
#include "vtkInformationVector.h"
#include "vtkInformation.h"
#include "vtkAlgorithm.h"
#include "vtkAlgorithmOutput.h"

vtkStandardNewMacro(vtkPVDataRepresentationPipeline);
//----------------------------------------------------------------------------
vtkPVDataRepresentationPipeline::vtkPVDataRepresentationPipeline()
{
}

//----------------------------------------------------------------------------
vtkPVDataRepresentationPipeline::~vtkPVDataRepresentationPipeline()
{
}

//----------------------------------------------------------------------------
int vtkPVDataRepresentationPipeline::ForwardUpstream(
  int i, int j, vtkInformation* request)
{
  vtkPVDataRepresentation* representation =
    vtkPVDataRepresentation::SafeDownCast(this->Algorithm);
  if (representation && representation->GetUsingCacheForUpdate())
    {
    // shunt upstream updates when using cache.
    return 1;
    }

  return this->Superclass::ForwardUpstream(i, j, request);
}

//----------------------------------------------------------------------------
int vtkPVDataRepresentationPipeline::ForwardUpstream(vtkInformation* request)
{
  vtkPVDataRepresentation* representation =
    vtkPVDataRepresentation::SafeDownCast(this->Algorithm);
  if (representation && representation->GetUsingCacheForUpdate())
    {
    // shunt upstream updates when using cache.
    return 1;
    }

  return this->Superclass::ForwardUpstream(request);
}

//----------------------------------------------------------------------------
int vtkPVDataRepresentationPipeline::ProcessRequest(vtkInformation* request,
  vtkInformationVector** inInfoVec,
  vtkInformationVector* outInfoVec)
{
  int ret_val = this->Superclass::ProcessRequest(request, inInfoVec,
    outInfoVec);
  return ret_val;
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentationPipeline::ExecuteDataEnd(vtkInformation* request,
    vtkInformationVector** inInfoVec,
    vtkInformationVector* outInfoVec)
{
  this->Superclass::ExecuteDataEnd(request, inInfoVec, outInfoVec);
}

//----------------------------------------------------------------------------
int vtkPVDataRepresentationPipeline::NeedToExecuteData(int outputPort,
    vtkInformationVector** inInfoVec, vtkInformationVector* outInfoVec)
{
  return this->Superclass::NeedToExecuteData(outputPort, inInfoVec, outInfoVec);
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentationPipeline::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

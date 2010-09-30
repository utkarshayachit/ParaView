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
  this->UpdateTime = 0.0;
  this->UpdateTimeValid = false;
}

//----------------------------------------------------------------------------
vtkPVDataRepresentationPipeline::~vtkPVDataRepresentationPipeline()
{
}

//----------------------------------------------------------------------------
void vtkPVDataRepresentationPipeline::Hack()
{
  vtkPVDataRepresentation::SafeDownCast(this->Algorithm)->SetUpdateTime(this->UpdateTime);
  vtkPVDataRepresentation::SafeDownCast(this->Algorithm)->SetUpdateTimeValid(this->UpdateTimeValid);
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
  this->LastTime = this->UpdateTime;
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
  os << indent << "UpdateTimeValid: " << this->UpdateTimeValid << endl;
  os << indent << "UpdateTime: " << this->UpdateTime << endl;
}

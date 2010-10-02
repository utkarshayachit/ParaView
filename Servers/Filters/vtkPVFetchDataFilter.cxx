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
#include "vtkPVFetchDataFilter.h"

#include "vtkDataObject.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkInstantiator.h"
#include "vtkObjectFactory.h"
#include "vtkReductionFilter.h"
#include "vtkSmartPointer.h"

vtkStandardNewMacro(vtkPVFetchDataFilter);
//----------------------------------------------------------------------------
vtkPVFetchDataFilter::vtkPVFetchDataFilter()
{
  this->PostGatherHelperName = NULL;
  this->PreGatherHelperName = NULL;
}

//----------------------------------------------------------------------------
vtkPVFetchDataFilter::~vtkPVFetchDataFilter()
{
  this->SetPostGatherHelperName(NULL);
  this->SetPreGatherHelperName(NULL);
}

//----------------------------------------------------------------------------
int vtkPVFetchDataFilter::RequestData(vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector)
{
  vtkSmartPointer<vtkDataObject> realInput;
  if (inputVector[0]->GetNumberOfInformationObjects())
    {
    vtkReductionFilter* reducer = vtkReductionFilter::New();
    vtkSmartPointer<vtkObject> foo;
    if (this->PreGatherHelperName)
      {
      foo.TakeReference(
        vtkInstantiator::CreateInstance(this->PreGatherHelperName));
      reducer->SetPreGatherHelper(vtkAlgorithm::SafeDownCast(foo));
      }
    if (this->PostGatherHelperName)
      {
      foo.TakeReference(
        vtkInstantiator::CreateInstance(this->PostGatherHelperName));
      reducer->SetPostGatherHelper(vtkAlgorithm::SafeDownCast(foo));
      }
    vtkDataObject* input = vtkDataObject::GetData(inputVector[0], 0);
    vtkDataObject* clone = input->NewInstance();
    clone->ShallowCopy(input);
    reducer->SetInput(input);
    clone->FastDelete();

    reducer->Update();
    clone = reducer->GetOutputDataObject(0)->NewInstance();
    clone->ShallowCopy(reducer->GetOutputDataObject(0));
    reducer->Delete();

    // replace input before calling superclass.
    realInput = input;
    inputVector[0]->GetInformationObject(0)->Set(
      vtkDataObject::DATA_OBJECT(), clone);
    clone->FastDelete();
    }

  int ret = this->Superclass::RequestData(request, inputVector, outputVector);
  if (realInput)
    {
    inputVector[0]->GetInformationObject(0)->Set(
      vtkDataObject::DATA_OBJECT(), realInput);
    }
  return ret;
}

//----------------------------------------------------------------------------
void vtkPVFetchDataFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

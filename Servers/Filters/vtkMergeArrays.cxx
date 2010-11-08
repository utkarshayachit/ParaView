/*=========================================================================

  Program:   ParaView
  Module:    vtkMergeArrays.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMergeArrays.h"

#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"

#include <vtkstd/set>
#include <vtkstd/string>
#include <vtksys/ios/sstream>

vtkStandardNewMacro(vtkMergeArrays);

namespace
{
  struct vtkMyValue;
    {
    vtkAbstractArray* Array;
    vtkstd::string ArrayName;
    int InputNumber;
    };
  typedef vtkstd::map<vtkstd::string, vtkMyValue> MyValuesMap;
};

//----------------------------------------------------------------------------
vtkMergeArrays::vtkMergeArrays()
{
  this->MangleArraysWithDuplicateNames = true;
}

//----------------------------------------------------------------------------
vtkMergeArrays::~vtkMergeArrays()
{
}

//----------------------------------------------------------------------------
int vtkMergeArrays::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  return 1;
}


//----------------------------------------------------------------------------
// Append data sets into single unstructured grid
int vtkMergeArrays::RequestData(vtkInformation *vtkNotUsed(request),
                                vtkInformationVector **inputVector,
                                vtkInformationVector *outputVector)
{
  int numCells, numPoints;
  int numArrays, arrayIdx;
  vtkDataSet *input;
  vtkDataArray *array;
  int num = inputVector[0]->GetNumberOfInformationObjects();
  if (num < 1)
    {
    return 0;
    }

  // get the output info object
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  vtkDataSet *output = vtkDataSet::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  input = vtkDataSet::SafeDownCast(
    inInfo->Get(vtkDataObject::DATA_OBJECT()));

  numCells = input->GetNumberOfCells();
  numPoints = input->GetNumberOfPoints();
  output->CopyStructure(input);
  for (int attributeType = 0;
    attributeType < vtkDataObject::NUMBER_OF_ATTRIBUTE_TYPES;
    attributeType++)
    {
    MyValuesMap array_map;
    for (int idx = 0; idx < num; ++idx)
      {
      input = vtkDataSet::GetData(inputVector[0], idx);
      if (input->GetNumberOfPoints() == numPoints &&
        input->GetNumberOfCells() == numCells)
        {
        vtkFieldData* inDsa = input->GetAttributesAsFieldData(attributeType);
        if (!inDsa)
          {
          continue;
          }
        for (int cc=0; cc < inDsa->GetNumberOfArrays(); cc++)
          {
          vtkAbstractArray* array = inDsa->GetAbstractArray(cc);
          if (array == NULL || !array->GetName())
            {
            vtkDebugMacro("Skipping arrays with no name");
            continue;
            }
          vtkMyValue value;
          value.Array= array;
          value.ArrayName = array->GetName();
          value.InputNumber = cc;

          MyValuesMap::iterator iter = array_map.find(array->GetName());
          if (iter == array_map.end())
            {
            }
          else if (this->MangleArraysWithDuplicateNames)
            {
            vtksys_ios::ostringstream stream;
            stream << array->GetName() << "_" << cc;
            value.ArrayName = stream.str();
            array_map[value.ArrayName] = value;

            // FIXME: change the name of the original hit as well.
            }
          array_map[value.ArrayName] = value;
          }
        }
      }

    input = vtkDataSet::GetData(inputVector[0], 0);
    vtkFieldData* inDsa = input->GetAttributesAsFieldData(attributeType);
    vtkFieldData* outDsa = output->GetAttributesAsFieldData(attributeType);
    if (!inDsa || !outDsa)
      {
      continue;
      }
    outDsa->PassData(inDsa);
    }

  return 1;
}

//----------------------------------------------------------------------------
void vtkMergeArrays::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "MangleArraysWithDuplicateNames: " <<
    this->MangleArraysWithDuplicateNames << endl;
}

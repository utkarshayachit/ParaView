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
#include "vtkPVRepresentedDataInformation.h"

#include "vtkDataObject.h"
#include "vtkObjectFactory.h"
#include "vtkPVDataRepresentation.h"

vtkStandardNewMacro(vtkPVRepresentedDataInformation);
//----------------------------------------------------------------------------
vtkPVRepresentedDataInformation::vtkPVRepresentedDataInformation()
{
}

//----------------------------------------------------------------------------
vtkPVRepresentedDataInformation::~vtkPVRepresentedDataInformation()
{
}

//----------------------------------------------------------------------------
void vtkPVRepresentedDataInformation::CopyFromObject(vtkObject* object)
{
  vtkPVDataRepresentation* repr = vtkPVDataRepresentation::SafeDownCast(object);
  if (repr)
    {
    this->Superclass::CopyFromObject(repr->GetRenderedDataObject(0));
    }
}

//----------------------------------------------------------------------------
void vtkPVRepresentedDataInformation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

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
#include "vtkCompositeRepresentation.h"

#include "vtkObjectFactory.h"
#include "vtkWeakPointer.h"
#include "vtkDataRepresentation.h"
#include "vtkView.h"
#include "vtkInformationVector.h"
#include "vtkInformation.h"
#include "vtkSmartPointer.h"

#include <vtkstd/map>
#include <assert.h>
class vtkCompositeRepresentation::vtkInternals
{
public:
  typedef vtkstd::map<int, vtkSmartPointer<vtkDataRepresentation> >
    RepresentationMap;
  RepresentationMap Representations;
  vtkWeakPointer<vtkView> View;
};

vtkStandardNewMacro(vtkCompositeRepresentation);
//----------------------------------------------------------------------------
vtkCompositeRepresentation::vtkCompositeRepresentation()
{
  this->Active = -1;
  this->Internals = new vtkInternals();
}

//----------------------------------------------------------------------------
vtkCompositeRepresentation::~vtkCompositeRepresentation()
{
  delete this->Internals;
  this->Internals = 0;
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::AddRepresentation(
  int key, vtkDataRepresentation* repr)
{
  assert(repr != NULL);

  if (this->Internals->Representations.find(key) !=
    this->Internals->Representations.end())
    {
    vtkWarningMacro("Replacing existing representation for key: "<< key);
    }
  this->Internals->Representations[key] = repr;
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::RemoveRepresentation(int key)
{
  vtkInternals::RepresentationMap::iterator iter =
    this->Internals->Representations.find(key);
  if (iter != this->Internals->Representations.end())
    {
    this->Internals->Representations.erase(iter);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::RemoveRepresentation(
  vtkDataRepresentation* repr)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); ++iter)
    {
    if (iter->second.GetPointer() == repr)
      {
      this->Internals->Representations.erase(iter);
      break;
      }
    }
}

//----------------------------------------------------------------------------
vtkDataRepresentation* vtkCompositeRepresentation::GetActiveRepresentation()
{
  vtkInternals::RepresentationMap::iterator iter =
    this->Internals->Representations.find(this->Active);
  if (iter != this->Internals->Representations.end())
    {
    return iter->second;
    }
  return NULL;
}

//----------------------------------------------------------------------------
int vtkCompositeRepresentation::FillInputPortInformation(
  int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkCompositeRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  vtkDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    if (request_type != vtkView::REQUEST_UPDATE())
      {
      return activeRepr->ProcessViewRequest(request_type, inInfo, outInfo);
      }
    }
  return this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
}

//----------------------------------------------------------------------------
int vtkCompositeRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    activeRepr->SetInputConnection(this->GetInternalOutputPort());
    }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkCompositeRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    activeRepr->ProcessRequest(request, inputVector, outputVector);
    }

  return this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkCompositeRepresentation::AddToView(vtkView* view)
{
  this->Internals->View = view;
  vtkDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    activeRepr->AddToView(view);
    }
  return this->Superclass::AddToView(view);
}

//----------------------------------------------------------------------------
bool vtkCompositeRepresentation::RemoveFromView(vtkView* view)
{
  vtkDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    activeRepr->RemoveFromView(view);
    }
  this->Internals->View = 0;
  return this->Superclass::RemoveFromView(view);
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::SetActive(int key)
{
  if (this->Active != key)
    {
    vtkDataRepresentation* curActive = this->GetActiveRepresentation();
    if (curActive && this->Internals->View)
      {
      curActive->RemoveFromView(this->Internals->View);
      }

    this->Active = key;
    this->Modified();

    curActive = this->GetActiveRepresentation();
    if (curActive && this->Internals->View)
      {
      curActive->AddToView(this->Internals->View);
      }
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

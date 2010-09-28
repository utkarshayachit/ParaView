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

#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkView.h"
#include "vtkWeakPointer.h"

#include <vtkstd/map>
#include <vtkstd/string>

#include <assert.h>

class vtkCompositeRepresentation::vtkInternals
{
public:
  typedef vtkstd::map<vtkstd::string, vtkSmartPointer<vtkPVDataRepresentation> >
    RepresentationMap;
  RepresentationMap Representations;

  vtkstd::string ActiveRepresentationKey;

  vtkWeakPointer<vtkView> View;
};

vtkStandardNewMacro(vtkCompositeRepresentation);
//----------------------------------------------------------------------------
vtkCompositeRepresentation::vtkCompositeRepresentation()
{
  this->Internals = new vtkInternals();
  this->Observer = vtkMakeMemberFunctionCommand(*this,
    &vtkCompositeRepresentation::TriggerUpdateDataEvent);
}

//----------------------------------------------------------------------------
vtkCompositeRepresentation::~vtkCompositeRepresentation()
{
  delete this->Internals;
  this->Internals = 0;
  this->Observer->Delete();
  this->Observer = 0;
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::SetVisibility(bool visible)
{
  this->Superclass::SetVisibility(visible);
  vtkPVDataRepresentation* repr = this->GetActiveRepresentation();
  if (repr)
    {
    repr->SetVisibility(visible);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::AddRepresentation(
  const char* key, vtkPVDataRepresentation* repr)
{
  assert(repr != NULL && key != NULL);

  if (this->Internals->Representations.find(key) !=
    this->Internals->Representations.end())
    {
    vtkWarningMacro("Replacing existing representation for key: "<< key);
    this->Internals->Representations[key]->RemoveObserver(this->Observer);
    }

  this->Internals->Representations[key] = repr;
  repr->AddObserver(vtkCommand::UpdateDataEvent, this->Observer);
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::RemoveRepresentation(const char* key)
{
  assert(key != NULL);

  vtkInternals::RepresentationMap::iterator iter =
    this->Internals->Representations.find(key);
  if (iter != this->Internals->Representations.end())
    {
    iter->second.GetPointer()->RemoveObserver(this->Observer);
    this->Internals->Representations.erase(iter);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::RemoveRepresentation(
  vtkPVDataRepresentation* repr)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); ++iter)
    {
    if (iter->second.GetPointer() == repr)
      {
      iter->second.GetPointer()->RemoveObserver(this->Observer);
      this->Internals->Representations.erase(iter);
      break;
      }
    }
}

//----------------------------------------------------------------------------
vtkPVDataRepresentation* vtkCompositeRepresentation::GetActiveRepresentation()
{
  vtkInternals::RepresentationMap::iterator iter =
    this->Internals->Representations.find(this->Internals->ActiveRepresentationKey);
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
void vtkCompositeRepresentation::SetInputConnection(
  int port, vtkAlgorithmOutput* input)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); iter++)
    {
    iter->second.GetPointer()->SetInputConnection(port, input);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::SetInputConnection(vtkAlgorithmOutput* input)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); iter++)
    {
    iter->second.GetPointer()->SetInputConnection(input);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::AddInputConnection(
  int port, vtkAlgorithmOutput* input)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); iter++)
    {
    iter->second.GetPointer()->AddInputConnection(port, input);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::AddInputConnection(vtkAlgorithmOutput* input)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); iter++)
    {
    iter->second.GetPointer()->AddInputConnection(input);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::RemoveInputConnection(
  int port, vtkAlgorithmOutput* input)
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); iter++)
    {
    iter->second.GetPointer()->RemoveInputConnection(port, input);
    }
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::MarkModified()
{
  vtkInternals::RepresentationMap::iterator iter;
  for (iter = this->Internals->Representations.begin();
    iter != this->Internals->Representations.end(); iter++)
    {
    iter->second.GetPointer()->MarkModified();
    }
}

//----------------------------------------------------------------------------
vtkSelection* vtkCompositeRepresentation::ConvertSelection(
  vtkView* view, vtkSelection* selection)
{
  vtkPVDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    return activeRepr->ConvertSelection(view, selection);
    }

  return this->Superclass::ConvertSelection(view, selection);
}

//----------------------------------------------------------------------------
vtkDataObject* vtkCompositeRepresentation::GetRenderedDataObject(int port)
{
  vtkPVDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    return activeRepr->GetRenderedDataObject(port);
    }

  return this->Superclass::GetRenderedDataObject(port);
}

//----------------------------------------------------------------------------
bool vtkCompositeRepresentation::AddToView(vtkView* view)
{
  this->Internals->View = view;
  vtkPVDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    view->AddRepresentation(activeRepr);
    }
  return this->Superclass::AddToView(view);
}

//----------------------------------------------------------------------------
bool vtkCompositeRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVDataRepresentation* activeRepr = this->GetActiveRepresentation();
  if (activeRepr)
    {
    view->RemoveRepresentation(activeRepr);
    }
  this->Internals->View = 0;
  return this->Superclass::RemoveFromView(view);
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::TriggerUpdateDataEvent()
{
  // We fire UpdateDataEvent to notify the representation proxy that the
  // representation was updated. The representation proxty will then call
  // PostUpdateData(). We do this since now representations are not updated at
  // the proxy level.
  this->InvokeEvent(vtkCommand::UpdateDataEvent);
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::SetActiveRepresentation(const char* key)
{
  assert(key != NULL);

  vtkPVDataRepresentation* curActive = this->GetActiveRepresentation();
  this->Internals->ActiveRepresentationKey = key;
  vtkPVDataRepresentation* newActive = this->GetActiveRepresentation();
  if (curActive != newActive)
    {
    if (curActive && this->Internals->View)
      {
      this->Internals->View->RemoveRepresentation(curActive);
      }

    if (newActive && this->Internals->View)
      {
      this->Internals->View->AddRepresentation(newActive);
      }

    if (newActive)
      {
      newActive->SetVisibility(this->GetVisibility());
      }
    }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkCompositeRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

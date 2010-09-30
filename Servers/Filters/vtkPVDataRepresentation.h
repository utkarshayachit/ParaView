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
// .NAME vtkPVDataRepresentation
// .SECTION Description
// vtkPVDataRepresentation adds some ParaView specific API to data
// representations.

#ifndef __vtkPVDataRepresentation_h
#define __vtkPVDataRepresentation_h

#include "vtkDataRepresentation.h"

class vtkInformationRequestKey;

class VTK_EXPORT vtkPVDataRepresentation : public vtkDataRepresentation
{
public:
  vtkTypeMacro(vtkPVDataRepresentation, vtkDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
  // typically called by the vtkView to request meta-data from the
  // representations or ask them to perform certain tasks e.g.
  // PrepareForRendering.
  // Overridden to skip processing when visibility if off.
  virtual int ProcessViewRequest(vtkInformationRequestKey* request_type,
    vtkInformation* inInfo, vtkInformation* outInfo);

  // Description:
  // This needs to be called on all instances of vtkGeometryRepresentation when
  // the input is modified. This is essential since the geometry filter does not
  // have any real-input on the client side which messes with the Update
  // requests.
  virtual void MarkModified() = 0;

  // Description:
  // Get/Set the visibility for this representation. When the visibility of
  // representation of false, all view passes are ignored.
  virtual void SetVisibility(bool val)
    { this->Visibility = val; }
  vtkGetMacro(Visibility, bool);

  // Description:
  // Returns the data object that is rendered from the given input port.
  virtual vtkDataObject* GetRenderedDataObject(int vtkNotUsed(port))
    { return this->GetInputDataObject(0, 0); }

  // Description:
  // Set the update time.
  void SetUpdateTime(double time);
  vtkGetMacro(UpdateTime, double);

  // Description:
  // Set whether the UpdateTime is valid.
  vtkGetMacro(UpdateTimeValid, bool);

//BTX
protected:
  vtkPVDataRepresentation();
  ~vtkPVDataRepresentation();

  // Description:
  // Create a default executive.
  virtual vtkExecutive* CreateDefaultExecutive();

  // Description:
  // Overridden to invoke vtkCommand::UpdateDataEvent.
  virtual int RequestData(vtkInformation*,
    vtkInformationVector**, vtkInformationVector*);

  virtual int RequestUpdateExtent(vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector);

  double UpdateTime;
  bool UpdateTimeValid;
private:
  vtkPVDataRepresentation(const vtkPVDataRepresentation&); // Not implemented
  void operator=(const vtkPVDataRepresentation&); // Not implemented

  bool Visibility;
//ETX
};

#endif

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
// .NAME vtkCompositeRepresentation - combine multiple representations into one
// with only 1 representation active at a time.
// .SECTION Description
// vtkCompositeRepresentation makes is possible to combine multiple
// representations into one. Only one representation can be active at a give
// time. vtkCompositeRepresentation provides API to add the representations that
// form the composite and to pick the active representation.
//
// vtkCompositeRepresentation relies on call AddToView and RemoveFromView
// on the internal representations whenever it needs to change the active
// representation. So it is essential that representations handle those methods
// correctly and don't suffer from uncanny side effects when that's done
// repeatedly.

#ifndef __vtkCompositeRepresentation_h
#define __vtkCompositeRepresentation_h

#include "vtkDataRepresentation.h"

class VTK_EXPORT vtkCompositeRepresentation : public vtkDataRepresentation
{
public:
  static vtkCompositeRepresentation* New();
  vtkTypeMacro(vtkCompositeRepresentation, vtkDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Add/Remove representations. \c key is a unique integer used to identify
  // that representation. Note it's a key not an index so removing
  // representations does not invalid other keys.
  void AddRepresentation(int key, vtkDataRepresentation* repr);
  void RemoveRepresentation(vtkDataRepresentation* repr);
  void RemoveRepresentation(int key);

  // Description:
  // Set the active key. If a valid key is not specified, then none of the
  // representations is treated as active.
  void SetActive(int key);

  // Description:
  // Returns the active representation's key.
  vtkGetMacro(Active, int);

  // Description:
  // Returns the active representation if valid.
  vtkDataRepresentation* GetActiveRepresentation();

  // Description:
  // Overridden to propagate to the active representation, whenever applicable.
  virtual int ProcessViewRequest(vtkInformationRequestKey* request_type,
    vtkInformation* inInfo, vtkInformation* outInfo);

//BTX
protected:
  vtkCompositeRepresentation();
  ~vtkCompositeRepresentation();

  // Description:
  // Since vtkCompositeRepresentation cannot know exactly what data types the
  // internal representations can take when this method gets called, we simply
  // pick vtkDataObject here and mark it optional as well. Internal
  // representations will start raising errors if that's not entirely true.
  virtual int FillInputPortInformation(int port, vtkInformation* info);

  // Description:
  // Overridden to propagate the RequestData() to the internal active
  // representation.
  virtual int RequestData(vtkInformation*,
    vtkInformationVector**, vtkInformationVector*);

  // Description:
  // Overridden to propagate the RequestUpdateExtent() to the internal active
  // representation.
  virtual int RequestUpdateExtent(vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector);

  // Description:
  // Adds the representation to the view.  This is called from
  // vtkView::AddRepresentation().  Subclasses should override this method.
  // Returns true if the addition succeeds.
  virtual bool AddToView(vtkView* view);

  // Description:
  // Removes the representation to the view.  This is called from
  // vtkView::RemoveRepresentation().  Subclasses should override this method.
  // Returns true if the removal succeeds.
  virtual bool RemoveFromView(vtkView* view);

  int Active;
private:
  vtkCompositeRepresentation(const vtkCompositeRepresentation&); // Not implemented
  void operator=(const vtkCompositeRepresentation&); // Not implemented

  class vtkInternals;
  vtkInternals* Internals;
//ETX
};

#endif

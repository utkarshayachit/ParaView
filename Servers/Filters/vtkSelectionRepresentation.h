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
// .NAME vtkSelectionRepresentation
// .SECTION Description
// vtkSelectionRepresentation is a representation to show the extracted
// cells. It uses vtkGeometryRepresentation and vtkDataRepresentation
// internally.

#ifndef __vtkSelectionRepresentation_h
#define __vtkSelectionRepresentation_h

#include "vtkDataRepresentation.h"

class vtkGeometryRepresentation;
class vtkDataLabelRepresentation;

class VTK_EXPORT vtkSelectionRepresentation : public vtkDataRepresentation
{
public:
  static vtkSelectionRepresentation* New();
  vtkTypeMacro(vtkSelectionRepresentation, vtkDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // One must change the internal representations only before the representation
  // is added to a view, after that it should not be touched.
  void SetLabelRepresentation(vtkDataLabelRepresentation*);

  // Description:
  // Overridden to simply pass the input to the internal representations. We
  // won't need this if vtkDataRepresentation correctly respected in the
  // arguments passed to it during ProcessRequest() etc.
  virtual void SetInputConnection(int port, vtkAlgorithmOutput* input);
  virtual void SetInputConnection(vtkAlgorithmOutput* input);
  virtual void AddInputConnection(int port, vtkAlgorithmOutput* input);
  virtual void AddInputConnection(vtkAlgorithmOutput* input);
  virtual void RemoveInputConnection(int port, vtkAlgorithmOutput* input);

  // Description:
  // This needs to be called on all instances of vtkSelectionRepresentation when
  // the input is modified. This is essential since the geometry filter does not
  // have any real-input on the client side which messes with the Update
  // requests.
  void MarkModified();

  // Description:
  // Forwarded to GeometryRepresentation.
  void SetColor(double r, double g, double b);
  void SetLineWidth(double val);
  void SetOpacity(double val);
  void SetPointSize(double val);
  void SetRepresentation(int val);
  void SetVisibility(int);
//BTX
protected:
  vtkSelectionRepresentation();
  ~vtkSelectionRepresentation();

  virtual int FillInputPortInformation(int port, vtkInformation* info);

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

  // Description:
  // Fires UpdateDataEvent
  void TriggerUpdateDataEvent();

  vtkGeometryRepresentation* GeometryRepresentation;
  vtkDataLabelRepresentation* LabelRepresentation;

private:
  vtkSelectionRepresentation(const vtkSelectionRepresentation&); // Not implemented
  void operator=(const vtkSelectionRepresentation&); // Not implemented
//ETX
};

#endif

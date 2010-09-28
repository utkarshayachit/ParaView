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
// .NAME vtkChartRepresentation
// .SECTION Description
//

#ifndef __vtkChartRepresentation_h
#define __vtkChartRepresentation_h

#include "vtkPVDataRepresentation.h"
#include "vtkWeakPointer.h"

class vtkPVContextView;
class vtkContextNamedOptions;
class vtkAnnotationLink;
class vtkClientServerMoveData;
class vtkReductionFilter;
class vtkBlockDeliveryPreprocessor;

class VTK_EXPORT vtkChartRepresentation : public vtkPVDataRepresentation
{
public:
  static vtkChartRepresentation* New();
  vtkTypeMacro(vtkChartRepresentation, vtkPVDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set the options object. This must be done before any other state is
  // updated.
  void SetOptions(vtkContextNamedOptions*);

  // Description:
  // Set visibility of the representation.
  virtual void SetVisibility(bool visible);

  // Description:
  // Get the number of series in this representation
  int GetNumberOfSeries();

  // Description:
  // Get the name of the series with the given index.  Returns 0 if the index
  // is out of range.  The returned pointer is only valid until the next call
  // to GetSeriesName.
  const char* GetSeriesName(int series);

  // Description:
  // vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
  // typically called by the vtkView to request meta-data from the
  // representations or ask them to perform certain tasks e.g.
  // PrepareForRendering.
  virtual int ProcessViewRequest(vtkInformationRequestKey* request_type,
    vtkInformation* inInfo, vtkInformation* outInfo);

  // Description:
  // This needs to be called on all instances of vtkGeometryRepresentation when
  // the input is modified. This is essential since the geometry filter does not
  // have any real-input on the client side which messes with the Update
  // requests.
  virtual void MarkModified();

  // *************************************************************************
  // Forwarded to vtkBlockDeliveryPreprocessor.
  void SetFieldAssociation(int);
  void SetCompositeDataSetIndex(unsigned int);

//BTX
protected:
  vtkChartRepresentation();
  ~vtkChartRepresentation();

  // Description:
  // Fill input port information.
  virtual int FillInputPortInformation(int port, vtkInformation* info);

  // Description:
  // Subclasses should override this to connect inputs to the internal pipeline
  // as necessary. Since most representations are "meta-filters" (i.e. filters
  // containing other filters), you should create shallow copies of your input
  // before connecting to the internal pipeline. The convenience method
  // GetInternalOutputPort will create a cached shallow copy of a specified
  // input for you. The related helper functions GetInternalAnnotationOutputPort,
  // GetInternalSelectionOutputPort should be used to obtain a selection or
  // annotation port whose selections are localized for a particular input data object.
  virtual int RequestData(vtkInformation*,
    vtkInformationVector**, vtkInformationVector*);

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

  vtkBlockDeliveryPreprocessor* Preprocessor;
  vtkReductionFilter* ReductionFilter;
  vtkClientServerMoveData* DeliveryFilter;
  vtkWeakPointer<vtkPVContextView> ContextView;
  vtkContextNamedOptions* Options;

  vtkAnnotationLink* AnnLink;
private:
  vtkChartRepresentation(const vtkChartRepresentation&); // Not implemented
  void operator=(const vtkChartRepresentation&); // Not implemented
//ETX
};

#endif

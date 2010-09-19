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
// .NAME vtkGeometryRepresentation
// .SECTION Description
// vtkGeometryRepresentation is a representation for showing polygon geometry.
// It handles non-polygonal datasets by extracting external surfaces.

#ifndef __vtkGeometryRepresentation_h
#define __vtkGeometryRepresentation_h

#include "vtkDataRepresentation.h"
#include "vtkProperty.h" // needed for VTK_POINTS etc.

class vtkOrderedCompositeDistributor;
class vtkPolyDataMapper;
class vtkPVGeometryFilter;
class vtkPVLODActor;
class vtkQuadricClustering;
class vtkScalarsToColors;
class vtkTexture;
class vtkUnstructuredDataDeliveryFilter;

class VTK_EXPORT vtkGeometryRepresentation : public vtkDataRepresentation
{
public:
  static vtkGeometryRepresentation* New();
  vtkTypeMacro(vtkGeometryRepresentation, vtkDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

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
  void MarkModified();

  enum AttributeTypes
    {
    POINT_DATA=0,
    CELL_DATA=1
    };

  // Description:
  // Enable/Disable LOD;
  void SetSuppressLOD(int){ } ;

  // Description:
  // Methods to control scalar coloring. ColorAttributeType defines the
  // attribute type.
  vtkSetMacro(ColorAttributeType, int);
  vtkGetMacro(ColorAttributeType, int);

  // Description:
  // Pick the array to color with.
  vtkSetStringMacro(ColorArrayName);
  vtkGetStringMacro(ColorArrayName);

  // Description:
  // Set the lighting properties of the object. vtkGeometryRepresentation
  // overrides these based of the following conditions:
  // \li When Representation is wireframe or points, it disables diffuse or
  // specular.
  // \li When scalar coloring is employed, it disabled specular.
  vtkSetMacro(Ambient, double);
  vtkSetMacro(Diffuse, double);
  vtkSetMacro(Specular, double);
  vtkGetMacro(Ambient, double);
  vtkGetMacro(Diffuse, double);
  vtkGetMacro(Specular, double);

  enum RepresentationTypes
    {
    POINTS = VTK_POINTS,
    WIREFRAME = VTK_WIREFRAME,
    SURFACE = VTK_SURFACE,
    SURFACE_WITH_EDGES = 3
    };

  // Description:
  // Set the representation type. This adds VTK_SURFACE_WITH_EDGES to those
  // defined in vtkProperty.
  vtkSetClampMacro(Representation, int, POINTS, SURFACE_WITH_EDGES);
  vtkGetMacro(Representation, int);

  //***************************************************************************
  // Forwarded to vtkProperty.
  void SetAmbientColor(double r, double g, double b);
  void SetBackfaceCulling(int val);
  void SetColor(double r, double g, double b);
  void SetDiffuseColor(double r, double g, double b);
  void SetEdgeColor(double r, double g, double b);
  void SetFrontfaceCulling(int val);
  void SetInterpolation(int val);
  void SetLineWidth(double val);
  void SetOpacity(double val);
  void SetPointSize(double val);
  void SetSpecularColor(double r, double g, double b);
  void SetSpecularPower(double val);

  //***************************************************************************
  // Forwarded to Actor.
  void SetOrientation(double, double, double);
  void SetOrigin(double, double, double);
  void SetPickable(int val);
  void SetPosition(double, double, double);
  void SetScale(double, double, double);
  void SetTexture(vtkTexture*);
  void SetVisibility(int val);

  //***************************************************************************
  // Forwarded to Mapper and LODMapper.
  void SetInterpolateScalarsBeforeMapping(int val);
  void SetLookupTable(vtkScalarsToColors* val);
  void SetMapScalars(int val);
  void SetStatic(int val);

//BTX
protected:
  vtkGeometryRepresentation();
  ~vtkGeometryRepresentation();

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
  // Produce meta-data about this representation that the view may find useful.
  bool GenerateMetaData(vtkInformation*, vtkInformation*);

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
  // Passes on parameters to vtkProperty and vtkMapper
  virtual void UpdateColoringParameters();

  vtkPVGeometryFilter* GeometryFilter;
  vtkQuadricClustering* Decimator;
  vtkPolyDataMapper* Mapper;
  vtkPolyDataMapper* LODMapper;
  vtkPVLODActor* Actor;
  vtkProperty* Property;
  vtkUnstructuredDataDeliveryFilter* DeliveryFilter;
  vtkUnstructuredDataDeliveryFilter* LODDeliveryFilter;
  vtkOrderedCompositeDistributor* Distributor;

  int ColorAttributeType;
  char* ColorArrayName;
  double Ambient;
  double Specular;
  double Diffuse;
  int Representation;

private:
  vtkGeometryRepresentation(const vtkGeometryRepresentation&); // Not implemented
  void operator=(const vtkGeometryRepresentation&); // Not implemented
//ETX
};

#endif


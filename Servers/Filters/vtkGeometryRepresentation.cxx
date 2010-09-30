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
#include "vtkGeometryRepresentation.h"

#include "vtkCommand.h"
#include "vtkCompositePolyDataMapper2.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkOrderedCompositeDistributor.h"
#include "vtkPKdTree.h"
#include "vtkProperty.h"
#include "vtkPVGeometryFilter.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkQuadricClustering.h"
#include "vtkRenderer.h"
#include "vtkSelectionConverter.h"
#include "vtkSelection.h"
#include "vtkSelectionNode.h"
#include "vtkUnstructuredDataDeliveryFilter.h"

//*****************************************************************************
class vtkGeometryRepresentationMultiBlockMaker : public vtkMultiBlockDataSetAlgorithm
{
public:
  static vtkGeometryRepresentationMultiBlockMaker* New();
  vtkTypeMacro(vtkGeometryRepresentationMultiBlockMaker, vtkMultiBlockDataSetAlgorithm);
protected:
  virtual int RequestData(vtkInformation *,
    vtkInformationVector ** inputVector, vtkInformationVector *outputVector)
    {
    vtkMultiBlockDataSet* inputMB =
      vtkMultiBlockDataSet::GetData(inputVector[0], 0);
    vtkMultiBlockDataSet* outputMB =
      vtkMultiBlockDataSet::GetData(outputVector, 0);
    if (inputMB)
      {
      outputMB->ShallowCopy(inputMB);
      return 1;
      }

    vtkDataObject* inputDO = vtkDataObject::GetData(inputVector[0], 0);
    vtkDataObject* clone = inputDO->NewInstance();
    clone->ShallowCopy(inputDO);
    outputMB->SetBlock(0, clone);
    clone->Delete();
    return 1;
    }

  virtual int FillInputPortInformation(int, vtkInformation *info)
    {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPolyData");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
    return 1;
    }
};
vtkStandardNewMacro(vtkGeometryRepresentationMultiBlockMaker);

//*****************************************************************************


vtkStandardNewMacro(vtkGeometryRepresentation);
//----------------------------------------------------------------------------
vtkGeometryRepresentation::vtkGeometryRepresentation()
{
  this->GeometryFilter = vtkPVGeometryFilter::New();
  this->MultiBlockMaker = vtkGeometryRepresentationMultiBlockMaker::New();
  this->Decimator = vtkQuadricClustering::New();
  this->Decimator->SetUseInputPoints(1);
  this->Decimator->SetCopyCellData(1);
  this->Decimator->SetUseInternalTriangles(0);
  this->Decimator->SetNumberOfDivisions(10, 10, 10);
  this->Mapper = vtkCompositePolyDataMapper2::New();
  this->LODMapper = vtkCompositePolyDataMapper2::New();
  this->Actor = vtkPVLODActor::New();
  this->Property = vtkProperty::New();
  //this->Property->SetOpacity(0.5);
  this->DeliveryFilter = vtkUnstructuredDataDeliveryFilter::New();
  this->LODDeliveryFilter = vtkUnstructuredDataDeliveryFilter::New();

  this->GeometryFilter->SetUseOutline(0);
  this->GeometryFilter->SetPassThroughCellIds(1);
  this->GeometryFilter->SetPassThroughPointIds(1);

  this->DeliveryFilter->SetOutputDataType(VTK_MULTIBLOCK_DATA_SET);
  this->LODDeliveryFilter->SetOutputDataType(VTK_MULTIBLOCK_DATA_SET);

  this->Distributor = vtkOrderedCompositeDistributor::New();
  this->Distributor->SetController(vtkMultiProcessController::GetGlobalController());
  this->Distributor->SetInputConnection(0, this->DeliveryFilter->GetOutputPort());
  this->Distributor->SetPassThrough(1);

  this->MultiBlockMaker->SetInputConnection(this->GeometryFilter->GetOutputPort());
  this->Decimator->SetInputConnection(this->MultiBlockMaker->GetOutputPort());
  this->Mapper->SetInputConnection(this->Distributor->GetOutputPort());
  this->LODMapper->SetInputConnection(this->LODDeliveryFilter->GetOutputPort());
  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetLODMapper(this->LODMapper);
  this->Actor->SetProperty(this->Property);

  this->ColorArrayName = 0;
  this->ColorAttributeType = VTK_SCALAR_MODE_DEFAULT;
  this->Ambient = 0.0;
  this->Diffuse = 1.0;
  this->Specular = 0.0;
  this->Representation = SURFACE;

  this->SuppressLOD = false;
}

//----------------------------------------------------------------------------
vtkGeometryRepresentation::~vtkGeometryRepresentation()
{
  this->GeometryFilter->Delete();
  this->MultiBlockMaker->Delete();
  this->Decimator->Delete();
  this->Mapper->Delete();
  this->LODMapper->Delete();
  this->Actor->Delete();
  this->Property->Delete();
  this->DeliveryFilter->Delete();
  this->LODDeliveryFilter->Delete();
  this->Distributor->Delete();
  this->SetColorArrayName(0);
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!this->GetVisibility())
    {
    return false;
    }

  if (request_type == vtkView::REQUEST_INFORMATION())
    {
    this->GenerateMetaData(inInfo, outInfo);
    }
  else if (request_type == vtkView::REQUEST_PREPARE_FOR_RENDER())
    {
    // In REQUEST_PREPARE_FOR_RENDER, we need to ensure all our data-deliver
    // filters have their states updated as requested by the view.

    // this is where we will look to see on what nodes are we going to render and
    // render set that up.
    this->DeliveryFilter->ProcessViewRequest(inInfo);
    this->LODDeliveryFilter->ProcessViewRequest(inInfo);
    bool lod = this->SuppressLOD? false : inInfo->Has(vtkPVRenderView::USE_LOD());
    if (lod)
      {
      if (inInfo->Has(vtkPVRenderView::LOD_RESOLUTION()))
        {
        int division = static_cast<int>(150 *
          inInfo->Get(vtkPVRenderView::LOD_RESOLUTION())) + 10;
        this->Decimator->SetNumberOfDivisions(division, division, division);
        }
      this->LODDeliveryFilter->Update();
      }
    else
      {
      this->DeliveryFilter->Update();
      }
    this->Actor->SetEnableLOD(lod? 1 : 0);
    }
  else if (request_type == vtkView::REQUEST_RENDER())
    {
    // typically, representations don't do anything special in this pass.
    // However, when we are doing ordered compositing, we need to ensure that
    // the redistribution of data happens in this pass.
    if (inInfo->Has(vtkPVRenderView::KD_TREE()))
      {
      vtkPKdTree* kdTree = vtkPKdTree::SafeDownCast(
        inInfo->Get(vtkPVRenderView::KD_TREE()));
      this->Distributor->SetPKdTree(kdTree);
      this->Distributor->SetPassThrough(0);
      }
    else
      {
      this->Distributor->SetPKdTree(NULL);
      this->Distributor->SetPassThrough(1);
      }

    this->UpdateColoringParameters();
    this->Actor->GetMapper()->Update();
    }

  return this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    this->GeometryFilter->SetInputConnection(
      this->GetInternalOutputPort());
    cout << "Update GeometryFilter" << endl;
    this->GeometryFilter->Update();
    this->MultiBlockMaker->SetInputConnection(
      this->GeometryFilter->GetOutputPort());
    this->DeliveryFilter->SetInputConnection(
      this->MultiBlockMaker->GetOutputPort());
    this->LODDeliveryFilter->SetInputConnection(
      this->Decimator->GetOutputPort());
    }
  else
    {
    cout << "Process has not input data" << endl;
    this->MultiBlockMaker->RemoveAllInputs();
    this->DeliveryFilter->RemoveAllInputs();
    this->LODDeliveryFilter->RemoveAllInputs();
    }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkGeometryRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  return this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
vtkDataObject* vtkGeometryRepresentation::GetRenderedDataObject(int port)
{
  (void) port;
  return this->Distributor->GetOutputDataObject(0);
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::GenerateMetaData(vtkInformation*,
  vtkInformation* outInfo)
{
  vtkDataObject* geom = this->GeometryFilter->GetOutputDataObject(0);
  if (geom)
    {
    outInfo->Set(vtkPVRenderView::GEOMETRY_SIZE(),geom->GetActualMemorySize());
    }

  outInfo->Set(vtkPVRenderView::REDISTRIBUTABLE_DATA_PRODUCER(),
    this->DeliveryFilter);
  if (this->Actor->GetProperty()->GetOpacity() < 1.0)
    {
    outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);
    }
  return 1;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::MarkModified()
{
  this->GeometryFilter->Modified();
  this->DeliveryFilter->Modified();
  this->LODDeliveryFilter->Modified();
  this->Distributor->Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::AddToView(vtkView* view)
{
  // FIXME: Need generic view API to add props.
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->AddActor(this->Actor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkGeometryRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->RemoveActor(this->Actor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::UpdateColoringParameters()
{
  bool using_scalar_coloring = false;
  if (this->ColorArrayName && this->ColorArrayName[0])
    {
    this->Mapper->SetScalarVisibility(1);
    this->LODMapper->SetScalarVisibility(1);
    this->Mapper->SelectColorArray(this->ColorArrayName);
    this->LODMapper->SelectColorArray(this->ColorArrayName);
    switch (this->ColorAttributeType)
      {
    case CELL_DATA:
      this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
      this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
      break;

    case POINT_DATA:
    default:
      this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
      this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
      break;
      }
    using_scalar_coloring = true;
    }
  else
    {
    this->Mapper->SetScalarVisibility(0);
    this->LODMapper->SetScalarVisibility(0);
    const char* null = NULL;
    this->Mapper->SelectColorArray(null);
    this->LODMapper->SelectColorArray(null);
    }

  // Adjust material properties.
  double diffuse = this->Diffuse;
  double specular = this->Specular;
  double ambient = this->Ambient;

  if (this->Representation != SURFACE &&
    this->Representation != SURFACE_WITH_EDGES)
    {
    diffuse = 0.0;
    ambient = 1.0;
    specular = 0.0;
    }
  else if (using_scalar_coloring)
    {
    // Disable specular highlighting is coloring by scalars.
    specular = 0.0;
    }

  this->Property->SetAmbient(ambient);
  this->Property->SetSpecular(specular);
  this->Property->SetDiffuse(diffuse);

  switch (this->Representation)
    {
  case SURFACE_WITH_EDGES:
    this->Property->SetEdgeVisibility(1);
    this->Property->SetRepresentation(VTK_SURFACE);
    break;

  default:
    this->Property->SetEdgeVisibility(0);
    this->Property->SetRepresentation(this->Representation);
    }
}

//----------------------------------------------------------------------------
vtkSelection* vtkGeometryRepresentation::ConvertSelection(
  vtkView* _view, vtkSelection* selection)
{
  vtkPVRenderView* view = vtkPVRenderView::SafeDownCast(_view);
  if (!view)
    {
    return this->Superclass::ConvertSelection(_view, selection);
    }

  vtkSelection* newInput = vtkSelection::New();

  // locate any selection nodes which belong to this representation.
  for (unsigned int cc=0; cc < selection->GetNumberOfNodes(); cc++)
    {
    vtkSelectionNode* node = selection->GetNode(cc);
    if (node->GetSelectedProp() == this->Actor)
      {
      newInput->AddNode(node);
      node->GetProperties()->Set(vtkSelectionNode::SOURCE(),
        this->GeometryFilter);
      }
    }

  if (newInput->GetNumberOfNodes() == 0)
    {
    newInput->Delete();
    return selection;
    }

  vtkSelection* output = vtkSelection::New();
  vtkSelectionConverter* convertor = vtkSelectionConverter::New();
  convertor->Convert(newInput, output, 0);
  convertor->Delete();
  newInput->Delete();
  return output;
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetVisibility(bool val)
{
  this->Actor->SetVisibility(val);
  this->Superclass::SetVisibility(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//****************************************************************************
// Methods merely forwarding parameters to internal objects.
//****************************************************************************

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetLookupTable(vtkScalarsToColors* val)
{
  this->Mapper->SetLookupTable(val);
  this->LODMapper->SetLookupTable(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetMapScalars(int val)
{
  this->Mapper->SetColorMode(val);
  this->LODMapper->SetColorMode(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetInterpolateScalarsBeforeMapping(int val)
{
  this->Mapper->SetInterpolateScalarsBeforeMapping(val);
  this->LODMapper->SetInterpolateScalarsBeforeMapping(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetStatic(int val)
{
  this->Mapper->SetStatic(val);
  this->LODMapper->SetStatic(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetColor(double r, double g, double b)
{
  this->Property->SetColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetLineWidth(double val)
{
  this->Property->SetLineWidth(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOpacity(double val)
{
  this->Property->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPointSize(double val)
{
  this->Property->SetPointSize(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetAmbientColor(double r, double g, double b)
{
  this->Property->SetAmbientColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetBackfaceCulling(int val)
{
  this->Property->SetBackfaceCulling(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetDiffuseColor(double r, double g, double b)
{
  this->Property->SetDiffuseColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetEdgeColor(double r, double g, double b)
{
  this->Property->SetEdgeColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetFrontfaceCulling(int val)
{
  this->Property->SetFrontfaceCulling(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetInterpolation(int val)
{
  this->Property->SetInterpolation(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetSpecularColor(double r, double g, double b)
{
  this->Property->SetSpecularColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetSpecularPower(double val)
{
  this->Property->SetSpecularPower(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOrientation(double x, double y, double z)
{
  this->Actor->SetOrientation(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetOrigin(double x, double y, double z)
{
  this->Actor->SetOrigin(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPickable(int val)
{
  this->Actor->SetPickable(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetPosition(double x, double y, double z)
{
  this->Actor->SetPosition(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetScale(double x, double y, double z)
{
  this->Actor->SetScale(x, y, z);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetTexture(vtkTexture* val)
{
  this->Actor->SetTexture(val);
}

//----------------------------------------------------------------------------
void vtkGeometryRepresentation::SetUseOutline(int val)
{
  this->GeometryFilter->SetUseOutline(val);
  this->Modified();
}

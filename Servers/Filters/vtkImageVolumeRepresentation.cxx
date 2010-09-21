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
#include "vtkImageVolumeRepresentation.h"

#include "vtkAlgorithmOutput.h"
#include "vtkCommand.h"
#include "vtkFixedPointVolumeRayCastMapper.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineSource.h"
#include "vtkPolyDataMapper.h"
#include "vtkPVLODVolume.h"
#include "vtkPVRenderView.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredDataDeliveryFilter.h"
#include "vtkVolumeProperty.h"

#include <vtkstd/map>
#include <vtkstd/string>

class vtkImageVolumeRepresentation::vtkInternals
{
public:
  typedef vtkstd::map<vtkstd::string, vtkSmartPointer<vtkVolumeMapper> >
    MapOfMappers;
  MapOfMappers Mappers;
};

vtkStandardNewMacro(vtkImageVolumeRepresentation);
//----------------------------------------------------------------------------
vtkImageVolumeRepresentation::vtkImageVolumeRepresentation()
{
  this->Internals = new vtkInternals();
  this->DefaultMapper = vtkFixedPointVolumeRayCastMapper::New();
  this->Property = vtkVolumeProperty::New();
  this->Actor = vtkPVLODVolume::New();
  this->Actor->SetProperty(this->Property);

  this->OutlineSource = vtkOutlineSource::New();
  this->OutlineDeliveryFilter = vtkUnstructuredDataDeliveryFilter::New();
  this->OutlineMapper = vtkPolyDataMapper::New();

  this->OutlineDeliveryFilter->SetInputConnection(
    this->OutlineSource->GetOutputPort());
  this->OutlineMapper->SetInputConnection(
    this->OutlineDeliveryFilter->GetOutputPort());
  this->Actor->SetLODMapper(this->OutlineMapper);

  this->ColorArrayName = 0;
  this->ActiveVolumeMapper = 0;
  this->Cache = vtkImageData::New();
}

//----------------------------------------------------------------------------
vtkImageVolumeRepresentation::~vtkImageVolumeRepresentation()
{
  delete this->Internals;
  this->DefaultMapper->Delete();
  this->Property->Delete();
  this->Actor->Delete();
  this->OutlineSource->Delete();
  this->OutlineDeliveryFilter->Delete();
  this->OutlineMapper->Delete();

  this->SetColorArrayName(0);
  this->SetActiveVolumeMapper(0);

  this->Cache->Delete();
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::AddVolumeMapper(
  const char* name, vtkVolumeMapper* mapper)
{
  this->Internals->Mappers[name] = mapper;
}

//----------------------------------------------------------------------------
vtkVolumeMapper* vtkImageVolumeRepresentation::GetActiveVolumeMapper()
{
  if (this->ActiveVolumeMapper)
    {
    vtkInternals::MapOfMappers::iterator iter =
      this->Internals->Mappers.find(this->ActiveVolumeMapper);
    if (iter != this->Internals->Mappers.end() && iter->second.GetPointer())
      {
      return iter->second.GetPointer();
      }
    }

  return this->DefaultMapper;
}

//----------------------------------------------------------------------------
int vtkImageVolumeRepresentation::FillInputPortInformation(
  int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageVolumeRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (request_type == vtkView::REQUEST_INFORMATION())
    {
    outInfo->Set(vtkPVRenderView::GEOMETRY_SIZE(),
      this->Cache->GetActualMemorySize());
    outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);
    outInfo->Set(vtkPVRenderView::REDISTRIBUTABLE_DATA_PRODUCER(),
      this->Cache->GetProducerPort()->GetProducer());
    }
  else if (request_type == vtkView::REQUEST_PREPARE_FOR_RENDER())
    {
    // // In REQUEST_PREPARE_FOR_RENDER, we need to ensure all our data-deliver
    // // filters have their states updated as requested by the view.

    // // this is where we will look to see on what nodes are we going to render and
    // // render set that up.
    this->OutlineDeliveryFilter->ProcessViewRequest(inInfo);
    this->OutlineDeliveryFilter->Update();
    }
  else if (request_type == vtkView::REQUEST_RENDER())
    {
    this->UpdateMapperParameters();
    }

  return this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
}

//----------------------------------------------------------------------------
int vtkImageVolumeRepresentation::RequestData(vtkInformation*,
    vtkInformationVector** inputVector, vtkInformationVector*)
{
  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    vtkImageData* input = vtkImageData::GetData(inputVector[0], 0);
    this->Cache->ShallowCopy(input);
    this->GetActiveVolumeMapper()->SetInput(this->Cache);
    this->Actor->SetEnableLOD(0);

    this->OutlineSource->SetBounds(input->GetBounds());
    }
  else
    {
    // when no input is present, it implies that this processes is on a node
    // without the data input i.e. either client or render-server, in which case
    // we show only the outline.
    this->Cache->Initialize();
    this->GetActiveVolumeMapper()->RemoveAllInputs();
    this->Actor->SetEnableLOD(1);
    }

  // We fire UpdateDataEvent to notify the representation proxy that the
  // representation was updated. The representation proxty will then call
  // PostUpdateData(). We do this since now representations are not updated at
  // the proxy level.
  this->InvokeEvent(vtkCommand::UpdateDataEvent);
  return 1;
}

//----------------------------------------------------------------------------
int vtkImageVolumeRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);

  // ideally, extent and time information will come from the view in
  // REQUEST_UPDATE(), include view-time.
  vtkMultiProcessController* controller =
    vtkMultiProcessController::GetGlobalController();
  if (controller && inputVector[0]->GetNumberOfInformationObjects() == 1)
    {
    vtkStreamingDemandDrivenPipeline* sddp =
      vtkStreamingDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
    sddp->SetUpdateExtent(inputVector[0]->GetInformationObject(0),
      controller->GetLocalProcessId(),
      controller->GetNumberOfProcesses(), 0);
    }
  return 1;
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::MarkModified()
{
  this->OutlineDeliveryFilter->Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
bool vtkImageVolumeRepresentation::AddToView(vtkView* view)
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
bool vtkImageVolumeRepresentation::RemoveFromView(vtkView* view)
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
void vtkImageVolumeRepresentation::UpdateMapperParameters()
{
  vtkVolumeMapper* activeMapper = this->GetActiveVolumeMapper();
  activeMapper->SelectScalarArray(this->ColorArrayName);
  switch (this->ColorAttributeType)
    {
  case CELL_DATA:
    activeMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
    break;

  case POINT_DATA:
  default:
    activeMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
    break;
    }
  this->Actor->SetMapper(activeMapper);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


//***************************************************************************
// Forwarded to Actor.

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetOrientation(double x, double y, double z)
{
  this->Actor->SetOrientation(x, y, z);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetOrigin(double x, double y, double z)
{
  this->Actor->SetOrigin(x, y, z);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetPickable(int val)
{
  this->Actor->SetPickable(val);
}
//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetPosition(double x , double y, double z)
{
  this->Actor->SetPosition(x, y, z);
}
//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetScale(double x, double y, double z)
{
  this->Actor->SetScale(x, y, z);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetVisibility(int val)
{
  this->Actor->SetVisibility(val);
}

//***************************************************************************
// Forwarded to vtkVolumeProperty.
//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetInterpolationType(int val)
{
  this->Property->SetInterpolationType(val);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetColor(vtkColorTransferFunction* lut)
{
  this->Property->SetColor(lut);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetScalarOpacity(vtkPiecewiseFunction* pwf)
{
  this->Property->SetScalarOpacity(pwf);
}

//----------------------------------------------------------------------------
void vtkImageVolumeRepresentation::SetScalarOpacityUnitDistance(double val)
{
  this->Property->SetScalarOpacityUnitDistance(val);
}

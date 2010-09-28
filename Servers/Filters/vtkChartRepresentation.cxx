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
#include "vtkChartRepresentation.h"

#include "vtkAnnotationLink.h"
#include "vtkBlockDeliveryPreprocessor.h"
#include "vtkClientServerMoveData.h"
#include "vtkCommand.h"
#include "vtkContextNamedOptions.h"
#include "vtkContextView.h"
#include "vtkDataObject.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPlot.h"
#include "vtkPVContextView.h"
#include "vtkPVMergeTables.h"
#include "vtkReductionFilter.h"
#include "vtkSelection.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTable.h"

vtkStandardNewMacro(vtkChartRepresentation);
vtkCxxSetObjectMacro(vtkChartRepresentation, Options, vtkContextNamedOptions);
//----------------------------------------------------------------------------
vtkChartRepresentation::vtkChartRepresentation()
{
  this->AnnLink = vtkAnnotationLink::New();
  this->Options = 0;

  this->Preprocessor = vtkBlockDeliveryPreprocessor::New();
  // Should the table be flattened (multicomponent columns split to single
  // component)?
  this->Preprocessor->SetFlattenTable(1);

  this->ReductionFilter = vtkReductionFilter::New();
  vtkPVMergeTables* post_gather_algo = vtkPVMergeTables::New();
  this->ReductionFilter->SetPostGatherHelper(post_gather_algo);
  this->ReductionFilter->SetController(vtkMultiProcessController::GetGlobalController());

  post_gather_algo->FastDelete();
  this->DeliveryFilter = vtkClientServerMoveData::New();
}

//----------------------------------------------------------------------------
vtkChartRepresentation::~vtkChartRepresentation()
{
  this->SetOptions(0);
  this->AnnLink->Delete();

  this->Preprocessor->Delete();
  this->ReductionFilter->Delete();
  this->DeliveryFilter->Delete();
}

//----------------------------------------------------------------------------
void vtkChartRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
bool vtkChartRepresentation::AddToView(vtkView* view)
{
  vtkPVContextView* chartView = vtkPVContextView::SafeDownCast(view);
  if (!chartView || chartView == this->ContextView)
    {
    return false;
    }

  this->ContextView = chartView;
  this->Options->SetChart(chartView->GetChart());
  this->Options->SetTableVisibility(this->GetVisibility());
  return true;
}

//----------------------------------------------------------------------------
bool vtkChartRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVContextView* chartView = vtkPVContextView::SafeDownCast(view);
  if (!chartView || chartView != this->ContextView)
    {
    return false;
    }

  this->Options->RemovePlotsFromChart();
  this->Options->SetChart(0);
  this->ContextView = 0;
  return true;
}

//----------------------------------------------------------------------------
int vtkChartRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkChartRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (request_type == vtkView::REQUEST_INFORMATION())
    {
    // nothing to do.
    }
  else if (request_type == vtkView::REQUEST_PREPARE_FOR_RENDER())
    {
    // In REQUEST_PREPARE_FOR_RENDER, we need to ensure all our data-deliver
    // filters have their states updated as requested by the view.

    // this is where we will look to see on what nodes are we going to render and
    // render set that up.
    this->ReductionFilter->Update();
    this->DeliveryFilter->Update();
    }
  else if (request_type == vtkView::REQUEST_RENDER())
    {
    // typically, representations don't do anything special in this pass.
    // However, when we are doing ordered compositing, we need to ensure that
    // the redistribution of data happens in this pass.
    this->Options->SetTable(vtkTable::SafeDownCast(
        this->DeliveryFilter->GetOutputDataObject(0)));
    }

  return this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
}

//----------------------------------------------------------------------------
int vtkChartRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  return this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkChartRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    this->Preprocessor->SetInputConnection(
      this->GetInternalOutputPort(0));
    this->Preprocessor->Update();
    this->ReductionFilter->SetInputConnection(this->Preprocessor->GetOutputPort());
    this->DeliveryFilter->SetInputConnection(this->ReductionFilter->GetOutputPort());
    }
  else
    {
    this->Preprocessor->RemoveAllInputs();
    this->ReductionFilter->RemoveAllInputs();
    this->DeliveryFilter->RemoveAllInputs();
    }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
void vtkChartRepresentation::MarkModified()
{
  this->ReductionFilter->Modified();
  this->DeliveryFilter->Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkChartRepresentation::SetVisibility(bool visible)
{
  this->Superclass::SetVisibility(visible);
  this->Options->SetTableVisibility(visible);
}

#ifdef FIXME
//----------------------------------------------------------------------------
void vtkChartRepresentation::Update(vtkSMViewProxy* view)
{
  this->Superclass::Update(view);

  // Update our selection
  this->SelectionRepresentation->Update(view);

  if (this->GetChart())
    {
    vtkSelection *sel =
        vtkSelection::SafeDownCast(this->SelectionRepresentation->GetOutput());
    this->AnnLink->SetCurrentSelection(sel);
    this->GetChart()->SetAnnotationLink(AnnLink);
    }

  // Set the table, in case it has changed.
  this->Options->SetTable(vtkTable::SafeDownCast(this->GetOutput()));

  this->UpdatePropertyInformation();
}
#endif

//----------------------------------------------------------------------------
int vtkChartRepresentation::GetNumberOfSeries()
{
  vtkTable *table = this->Options->GetTable();
  if (table)
    {
    return table->GetNumberOfColumns();
    }
  else
    {
    return 0;
    }
}

//----------------------------------------------------------------------------
const char* vtkChartRepresentation::GetSeriesName(int col)
{
  vtkTable *table = this->Options->GetTable();
  if (table)
    {
    return table->GetColumnName(col);
    }
  else
    {
    return NULL;
    }
}

// *************************************************************************
// Forwarded to vtkBlockDeliveryPreprocessor.
void vtkChartRepresentation::SetFieldAssociation(int v)
{
  this->Preprocessor->SetFieldAssociation(v);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkChartRepresentation::SetCompositeDataSetIndex(unsigned int v)
{
  this->Preprocessor->SetCompositeDataSetIndex(v);
  this->Modified();
}

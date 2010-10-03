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
#include "vtkPVCacheKeeper.h"
#include "vtkPVContextView.h"
#include "vtkPVMergeTables.h"
#include "vtkReductionFilter.h"
#include "vtkSelection.h"
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

  this->CacheKeeper = vtkPVCacheKeeper::New();

  this->ReductionFilter = vtkReductionFilter::New();
  vtkPVMergeTables* post_gather_algo = vtkPVMergeTables::New();
  this->ReductionFilter->SetPostGatherHelper(post_gather_algo);
  this->ReductionFilter->SetController(vtkMultiProcessController::GetGlobalController());

  this->CacheKeeper->SetInputConnection(this->Preprocessor->GetOutputPort());
  this->ReductionFilter->SetInputConnection(this->CacheKeeper->GetOutputPort());

  post_gather_algo->FastDelete();
  this->DeliveryFilter = vtkClientServerMoveData::New();
  this->DeliveryFilter->SetOutputDataType(VTK_TABLE);
}

//----------------------------------------------------------------------------
vtkChartRepresentation::~vtkChartRepresentation()
{
  this->SetOptions(0);
  this->AnnLink->Delete();

  this->Preprocessor->Delete();
  this->CacheKeeper->Delete();
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
  if (this->Options)
    {
    this->Options->SetChart(chartView->GetChart());
    this->Options->SetTableVisibility(this->GetVisibility());
    }
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

  if (this->Options)
    {
    this->Options->RemovePlotsFromChart();
    this->Options->SetChart(0);
    }
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
int vtkChartRepresentation::RequestUpdateExtent(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  return this->Superclass::RequestUpdateExtent(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
vtkTable* vtkChartRepresentation::GetLocalOutput()
{
  return vtkTable::SafeDownCast(this->DeliveryFilter->GetOutputDataObject(0));
}

//----------------------------------------------------------------------------
int vtkChartRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // Pass caching information to the cache keeper.
  this->CacheKeeper->SetCachingEnabled(this->GetUseCache());
  this->CacheKeeper->SetCacheTime(this->GetCacheKey());

  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    this->Preprocessor->SetInputConnection(
      this->GetInternalOutputPort(0));
    this->Preprocessor->Update();
    this->DeliveryFilter->SetInputConnection(this->ReductionFilter->GetOutputPort());
    }
  else
    {
    this->Preprocessor->RemoveAllInputs();
    this->DeliveryFilter->RemoveAllInputs();
    }

  this->DeliveryFilter->Update();
  if (this->Options)
    {
    this->Options->SetTable(this->GetLocalOutput());
    }
  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkChartRepresentation::IsCached(double cache_key)
{
  return this->CacheKeeper->IsCached(cache_key);
}

//----------------------------------------------------------------------------
void vtkChartRepresentation::MarkModified()
{
  this->ReductionFilter->Modified();
  this->DeliveryFilter->Modified();
  if (!this->GetUseCache())
    {
    // Cleanup caches when not using cache.
    this->CacheKeeper->RemoveAllCaches();
    }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkChartRepresentation::SetVisibility(bool visible)
{
  this->Superclass::SetVisibility(visible);
  if (this->Options)
    {
    this->Options->SetTableVisibility(visible);
    }
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
  if (this->Options)
    {
    this->Options->SetTable(vtkTable::SafeDownCast(this->GetOutput()));
    }

  this->UpdatePropertyInformation();
}
#endif

//----------------------------------------------------------------------------
int vtkChartRepresentation::GetNumberOfSeries()
{
  vtkTable *table = this->GetLocalOutput();
  if (table)
    {
    return table->GetNumberOfColumns();
    }

  return 0;
}

//----------------------------------------------------------------------------
const char* vtkChartRepresentation::GetSeriesName(int col)
{
  vtkTable *table = this->GetLocalOutput();
  if (table)
    {
    return table->GetColumnName(col);
    }

  return NULL;
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

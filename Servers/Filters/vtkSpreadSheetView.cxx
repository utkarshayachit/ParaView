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
#include "vtkSpreadSheetView.h"

#include "vtkAlgorithmOutput.h"
#include "vtkCharArray.h"
#include "vtkClientServerMoveData.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataSet.h"
#include "vtkEventForwarderCommand.h"
#include "vtkMarkSelectedRows.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkMultiProcessController.h"
#include "vtkMultiProcessStream.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVMergeTables.h"
#include "vtkPVOptions.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkReductionFilter.h"
#include "vtkRemoteConnection.h"
#include "vtkSmartPointer.h"
#include "vtkSortedTableStreamer.h"
#include "vtkSpreadSheetRepresentation.h"
#include "vtkTable.h"
#include "vtkVariant.h"

#include <vtkstd/map>

class vtkSpreadSheetView::vtkInternals
{
public:
  class CacheInfo
    {
  public:
    vtkSmartPointer<vtkTable> Dataobject;
    vtkTimeStamp RecentUseTime;
    };

  typedef vtkstd::map<vtkIdType, CacheInfo> CacheType;
  CacheType CachedBlocks;

  vtkTable* GetDataObject(vtkIdType blockId)
    {
    CacheType::iterator iter = this->CachedBlocks.find(blockId);
    return (iter != this->CachedBlocks.end()?
      iter->second.Dataobject.GetPointer() : NULL);
    }

  void AddToCache(vtkIdType blockId, vtkTable* data, vtkIdType max)
    {
    CacheType::iterator iter = this->CachedBlocks.find(blockId);
    if (iter != this->CachedBlocks.end())
      {
      this->CachedBlocks.erase(iter);
      }

    if (static_cast<vtkIdType>(this->CachedBlocks.size()) == max)
      {
      // remove least-recent-used block.
      iter = this->CachedBlocks.begin();
      CacheType::iterator iterToRemove = this->CachedBlocks.begin();
      for (; iter != this->CachedBlocks.end(); ++iter)
        {
        if (iterToRemove->second.RecentUseTime > iter->second.RecentUseTime)
          {
          iterToRemove = iter;
          }
        }
      this->CachedBlocks.erase(iterToRemove);
      }

    CacheInfo info;
    vtkTable* clone = vtkTable::New();
    clone->ShallowCopy(data);
    info.Dataobject = clone;
    clone->FastDelete();
    info.RecentUseTime.Modified();
    this->CachedBlocks[blockId] = info;
    }

  vtkWeakPointer<vtkSpreadSheetRepresentation> ActiveRepresentation;
  vtkEventForwarderCommand* Forwarder;
  vtkCommand* Observer;
};

namespace
{
  void FetchRMI(void *localArg,
    void *remoteArg, int remoteArgLength, int)
    {
    vtkMultiProcessStream stream;
    stream.SetRawData(
      reinterpret_cast<unsigned char*>(remoteArg), remoteArgLength);
    unsigned int id = 0;
    int blockid = -1;
    stream >> id >> blockid;
    vtkSpreadSheetView* self =
      reinterpret_cast<vtkSpreadSheetView*>(localArg);
    if (self->GetIdentifier() == id)
      {
      self->FetchBlockCallback(blockid);
      }
    }

  unsigned long vtkCountNumberOfRows(vtkDataObject* dobj)
    {
    vtkTable* table = vtkTable::SafeDownCast(dobj);
    if (table)
      {
      return table->GetNumberOfRows();
      }
    vtkCompositeDataSet* cd = vtkCompositeDataSet::SafeDownCast(dobj);
    if (cd)
      {
      unsigned long count = 0;
      vtkCompositeDataIterator* iter = cd->NewIterator();
      for (iter->InitTraversal(); !iter->IsDoneWithTraversal();
        iter->GoToNextItem())
        {
        count += vtkCountNumberOfRows(iter->GetCurrentDataObject());
        }
      iter->Delete();
      return count;
      }
    return 0;
    }

  vtkAlgorithmOutput* vtkGetDataProducer(
    vtkSpreadSheetView* self, vtkSpreadSheetRepresentation* repr)
    {
    if (repr)
      {
      if (self->GetShowExtractedSelection())
        {
        return repr->GetExtractedDataProducer();
        }
      else
        {
        return repr->GetDataProducer();
        }
      }
    return NULL;
    }

  vtkAlgorithmOutput* vtkGetSelectionProducer(
    vtkSpreadSheetView* self, vtkSpreadSheetRepresentation* repr)
    {
    if (repr)
      {
      if (self->GetShowExtractedSelection())
        {
        return NULL;
        }
      return repr->GetSelectionProducer();
      }
    return NULL;
    }
}

vtkStandardNewMacro(vtkSpreadSheetView);
//----------------------------------------------------------------------------
vtkSpreadSheetView::vtkSpreadSheetView()
{
  this->ShowExtractedSelection = false;
  this->TableStreamer = vtkSortedTableStreamer::New();
  this->TableSelectionMarker = vtkMarkSelectedRows::New();

  this->ReductionFilter = vtkReductionFilter::New();
  this->ReductionFilter->SetController(
    vtkMultiProcessController::GetGlobalController());

  vtkPVMergeTables* post_gather_algo = vtkPVMergeTables::New();
  this->ReductionFilter->SetPostGatherHelper(post_gather_algo);
  post_gather_algo->FastDelete();

  this->DeliveryFilter = vtkClientServerMoveData::New();
  this->DeliveryFilter->SetOutputDataType(VTK_TABLE);
  this->DeliveryFilter->SetProcessModuleConnection(
    vtkProcessModule::GetProcessModule()->GetActiveRemoteConnection());

  this->TableSelectionMarker->SetInputConnection(
    this->TableStreamer->GetOutputPort());
  this->ReductionFilter->SetInputConnection(
    this->TableSelectionMarker->GetOutputPort());

  this->Internals = new vtkInternals();
  this->Internals->Forwarder = vtkEventForwarderCommand::New();
  this->Internals->Forwarder->SetTarget(this);

  this->Internals->Observer = vtkMakeMemberFunctionCommand(*this,
    &vtkSpreadSheetView::OnRepresentationUpdated);
  this->SomethingUpdated = false;

  if (vtkProcessModule::GetProcessModule()->GetOptions()->GetProcessType() !=
    vtkPVOptions::PVRENDER_SERVER)
    {
    this->RMICallbackTag = this->SynchronizedWindows->AddRMICallback(
      ::FetchRMI, this, FETCH_BLOCK_TAG);
    }
}

//----------------------------------------------------------------------------
vtkSpreadSheetView::~vtkSpreadSheetView()
{
  this->SynchronizedWindows->RemoveRMICallback(this->RMICallbackTag);
  this->RMICallbackTag = 0;

  this->Internals->Forwarder->SetTarget(NULL);
  this->Internals->Forwarder->Delete();
  this->Internals->Observer->Delete();

  this->TableStreamer->Delete();
  this->TableSelectionMarker->Delete();
  this->ReductionFilter->Delete();
  this->DeliveryFilter->Delete();

  delete this->Internals;
  this->Internals = 0;
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::SetShowExtractedSelection(bool val)
{
  if (val != this->ShowExtractedSelection)
    {
    this->ShowExtractedSelection = val;
    this->ClearCache();
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::ClearCache()
{
  this->Internals->CachedBlocks.clear();
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::Update()
{
  vtkSpreadSheetRepresentation* prev = this->Internals->ActiveRepresentation;
  vtkSpreadSheetRepresentation* cur = NULL;
  for (int cc=0; cc < this->GetNumberOfRepresentations(); cc++)
    {
    vtkSpreadSheetRepresentation* repr = vtkSpreadSheetRepresentation::SafeDownCast(
      this->GetRepresentation(cc));
    if (repr && repr->GetVisibility())
      {
      cur = repr;
      break;
      }
    }

  if (prev != cur)
    {
    if (prev)
      {
      prev->RemoveObserver(this->Internals->Observer);
      }
    if (cur)
      {
      cur->AddObserver(vtkCommand::UpdateDataEvent, this->Internals->Observer);
      }
    this->Internals->ActiveRepresentation = cur;
    this->ClearCache();
    }

  this->SomethingUpdated = false;
  this->Superclass::Update();

  unsigned long num_rows = 0;

  // From the active representation obtain the data/selection producers that
  // need to be streamed to the client.
  vtkAlgorithmOutput* dataPort = vtkGetDataProducer(this, cur);
  vtkAlgorithmOutput* selectionPort = vtkGetSelectionProducer(this, cur);

  this->TableStreamer->SetInputConnection(dataPort);
  this->TableSelectionMarker->SetInputConnection(1, selectionPort);
  if (dataPort)
    {
    dataPort->GetProducer()->Update();
    this->DeliveryFilter->SetInputConnection(this->ReductionFilter->GetOutputPort());
    num_rows = vtkCountNumberOfRows(
      dataPort->GetProducer()->GetOutputDataObject(dataPort->GetIndex()));
    }
  else
    {
    this->DeliveryFilter->RemoveAllInputs();
    }

  if (cur)
    {
    this->SynchronizedWindows->SynchronizeSize(num_rows);
    }

  if (this->NumberOfRows != num_rows)
    {
    this->SomethingUpdated = true;
    }
  this->NumberOfRows = num_rows;
  if (this->SomethingUpdated)
    {
    this->InvokeEvent(vtkCommand::UpdateDataEvent);
    }
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::OnRepresentationUpdated()
{
  this->ClearCache();
  this->SomethingUpdated = true;
}

//----------------------------------------------------------------------------
vtkTable* vtkSpreadSheetView::FetchBlock(vtkIdType blockindex)
{
  vtkTable* block = this->Internals->GetDataObject(blockindex);
  if (!block)
    {
    vtkMultiProcessStream stream;
    stream << this->Identifier << static_cast<int>(blockindex);
    this->SynchronizedWindows->TriggerRMI(stream, FETCH_BLOCK_TAG);
    this->FetchBlockCallback(blockindex);
    block = vtkTable::SafeDownCast(
      this->DeliveryFilter->GetOutputDataObject(0));
    this->Internals->AddToCache(blockindex, block, 10);
    this->InvokeEvent(vtkCommand::UpdateEvent, &blockindex);
    }

  return block;
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::FetchBlockCallback(vtkIdType blockindex)
{
  this->TableStreamer->SetBlock(blockindex);

  this->TableStreamer->Modified();
  this->TableSelectionMarker->SetFieldAssociation(
    this->Internals->ActiveRepresentation->GetFieldAssociation());
  this->ReductionFilter->Modified();
  this->DeliveryFilter->Modified();
  this->DeliveryFilter->Update();
}

//----------------------------------------------------------------------------
vtkIdType vtkSpreadSheetView::GetNumberOfColumns()
{
  if (this->Internals->ActiveRepresentation)
    {
    vtkTable* block0 = this->FetchBlock(0);
    if (block0)
      {
      return block0->GetNumberOfColumns();
      }
    }
  return 0;
}

//----------------------------------------------------------------------------
vtkIdType vtkSpreadSheetView::GetNumberOfRows()
{
  return this->NumberOfRows;
}

//----------------------------------------------------------------------------
const char* vtkSpreadSheetView::GetColumnName(vtkIdType index)
{
  if (this->Internals->ActiveRepresentation)
    {
    vtkTable* block0 = this->FetchBlock(0);
    if (block0)
      {
      return block0->GetColumnName(index);
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
vtkVariant vtkSpreadSheetView::GetValue(vtkIdType row, vtkIdType col)
{
  vtkIdType blockSize = this->TableStreamer->GetBlockSize();
  vtkIdType blockIndex = row / blockSize;
  vtkTable* block = this->FetchBlock(blockIndex);
  vtkIdType blockOffset = row - (blockIndex * blockSize);
  return block->GetValue(blockOffset, col);
}

//----------------------------------------------------------------------------
bool vtkSpreadSheetView::IsRowSelected(vtkIdType row)
{
  vtkIdType blockSize = this->TableStreamer->GetBlockSize();
  vtkIdType blockIndex = row / blockSize;
  vtkTable* block = this->FetchBlock(blockIndex);
  vtkIdType blockOffset = row - (blockIndex * blockSize);
  vtkCharArray* __vtkIsSelected__ = vtkCharArray::SafeDownCast(
    block->GetColumnByName("__vtkIsSelected__"));
  if (__vtkIsSelected__)
    {
    return __vtkIsSelected__->GetValue(blockOffset) == 1;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkSpreadSheetView::IsAvailable(vtkIdType row)
{
  vtkIdType blockSize = this->TableStreamer->GetBlockSize();
  vtkIdType blockIndex = row / blockSize;
  return this->Internals->GetDataObject(blockIndex) != NULL;
}

//***************************************************************************
// Forwarded to vtkSortedTableStreamer.
//----------------------------------------------------------------------------
void vtkSpreadSheetView::SetColumnNameToSort(const char* name)
{
  this->TableStreamer->SetColumnNameToSort(name);
  this->ClearCache();
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::SetComponentToSort(int val)
{
  this->TableStreamer->SetSelectedComponent(val);
  this->ClearCache();
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::SetInvertSortOrder(bool val)
{
  this->TableStreamer->SetInvertOrder(val? 1 : 0);
  this->ClearCache();
}

//----------------------------------------------------------------------------
void vtkSpreadSheetView::SetBlockSize(vtkIdType val)
{
  this->TableStreamer->SetBlockSize(val);
  this->ClearCache();
}

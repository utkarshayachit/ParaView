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
#include "vtkPVContextView.h"

#include "vtkContextView.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkTimerLog.h"
#include "vtkRenderWindow.h"

//----------------------------------------------------------------------------
vtkPVContextView::vtkPVContextView()
{
  this->RenderWindow = this->SynchronizedWindows->NewRenderWindow();
  this->ContextView = vtkContextView::New();
  this->ContextView->SetRenderWindow(this->RenderWindow);
}

//----------------------------------------------------------------------------
vtkPVContextView::~vtkPVContextView()
{
  this->RenderWindow->Delete();
  this->ContextView->Delete();
}

//----------------------------------------------------------------------------
void vtkPVContextView::StillRender()
{
  vtkTimerLog::MarkStartEvent("Still Render");
  this->Render(false);
  vtkTimerLog::MarkEndEvent("Still Render");
}

//----------------------------------------------------------------------------
void vtkPVContextView::InteractiveRender()
{
  vtkTimerLog::MarkStartEvent("Interactive Render");
  this->Render(true);
  vtkTimerLog::MarkEndEvent("Interactive Render");
}

//----------------------------------------------------------------------------
void vtkPVContextView::Render(bool interactive)
{
  if (!interactive)
    {
    // Update all representations.
    // This should update mostly just the inputs to the representations, and maybe
    // the internal geometry filter.
    this->Update();

    // Do the vtkView::REQUEST_INFORMATION() pass.
    this->CallProcessViewRequest(vtkView::REQUEST_INFORMATION(),
      this->RequestInformation, this->ReplyInformationVector);
    }

  // Since currently we only support client-side rendering, we disable render
  // synchronization for charts among all processes.
  this->SynchronizedWindows->SetEnabled(false);

  // Build the request for REQUEST_PREPARE_FOR_RENDER().
  // Move all data to client.
  //this->RequestInformation->Set(DATA_DISTRIBUTION_MODE(),
  //  vtkMPIMoveData::COLLECT);

  // In REQUEST_PREPARE_FOR_RENDER, this view expects all representations to
  // know the data-delivery mode.
  this->CallProcessViewRequest(
    vtkView::REQUEST_PREPARE_FOR_RENDER(),
    this->RequestInformation, this->ReplyInformationVector);

  // This pass is typically used for post-delivery updates.
  this->CallProcessViewRequest(
    vtkView::REQUEST_RENDER(),
    this->RequestInformation, this->ReplyInformationVector);

  // Call Render() on local render window only on the client (or root node in
  // batch mode).
  if (this->SynchronizedWindows->GetLocalProcessIsDriver())
    {
    this->ContextView->Render();
    //this->GetRenderWindow()->Render();
    }
}

//----------------------------------------------------------------------------
void vtkPVContextView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

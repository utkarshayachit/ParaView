/*=========================================================================

   Program: ParaView
   Module:    pqLookupTableManager.cxx

   Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "pqLookupTableManager.h"

#include <QtDebug>

#include "pqApplicationCore.h"
#include "pqDataRepresentation.h"
#include "pqRenderViewBase.h"
#include "pqScalarOpacityFunction.h"
#include "pqScalarsToColors.h"
#include "pqServer.h"
#include "pqServerManagerModel.h"
#include "pqUndoStack.h"
#include "vtkNew.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMTransferFunctionManager.h"
#include "vtkSMTransferFunctionProxy.h"

//-----------------------------------------------------------------------------
pqLookupTableManager::pqLookupTableManager(QObject* _parent/*=0*/)
  : QObject(_parent)
{
}

//-----------------------------------------------------------------------------
pqLookupTableManager::~pqLookupTableManager()
{
}

//-----------------------------------------------------------------------------
pqScalarsToColors* pqLookupTableManager::getLookupTable(pqServer* server, 
  const QString& arrayname, int number_of_components, int)
{
  Q_UNUSED(number_of_components);
  vtkNew<vtkSMTransferFunctionManager> mgr;
  vtkSMProxy* proxy = mgr->GetColorTransferFunction(
    arrayname.toAscii().data(), server->proxyManager());
  if (proxy)
    {
    pqApplicationCore* core = pqApplicationCore::instance();
    pqServerManagerModel* smmodel = core->getServerManagerModel();
    return smmodel->findItem<pqScalarsToColors*>(proxy);
    }

  return NULL;
}

//-----------------------------------------------------------------------------
pqScalarOpacityFunction* pqLookupTableManager::getScalarOpacityFunction(
  pqServer* server, const QString& arrayname, int number_of_components, int)
{
  Q_UNUSED(number_of_components);
  vtkNew<vtkSMTransferFunctionManager> mgr;
  vtkSMProxy* proxy = mgr->GetOpacityTransferFunction(
    arrayname.toAscii().data(), server->proxyManager());
  if (proxy)
    {
    pqApplicationCore* core = pqApplicationCore::instance();
    pqServerManagerModel* smmodel = core->getServerManagerModel();
    return smmodel->findItem<pqScalarOpacityFunction*>(proxy);
    }

  return NULL;
}

//-----------------------------------------------------------------------------
void pqLookupTableManager::setScalarBarVisibility(pqDataRepresentation* repr,  bool visible)
{
  if (visible)
    {
    BEGIN_UNDO_SET("Show scalar bar");
    }
  else
    {
    BEGIN_UNDO_SET("Hide scalar bar");
    }
  pqView *view = repr->getView();  
  vtkSMPVRepresentationProxy::SetScalarBarVisibility(
    repr->getProxy(), view->getProxy(), visible);
  END_UNDO_SET();
}

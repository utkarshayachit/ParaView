/*=========================================================================

   Program: ParaView
   Module:    pqPipelineRepresentation.cxx

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

/// \file pqPipelineRepresentation.cxx
/// \date 4/24/2006

#include "pqPipelineRepresentation.h"


// ParaView Server Manager includes.
#include "vtkCommand.h"
#include "vtkDataObject.h"
#include "vtkEventQtSlotConnect.h"
#include "vtkGeometryRepresentation.h"
#include "vtkMath.h"
#include "vtkProcessModule.h"
#include "vtkProperty.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkPVTemporalDataInformation.h"
#include "vtkScalarsToColors.h"
#include "vtkSmartPointer.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMGlobalPropertiesManager.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMTransferFunctionProxy.h"

// Qt includes.
#include <QList>
#include <QPair>
#include <QPointer>
#include <QRegExp>
#include <QtDebug>

// ParaView includes.
#include "pqApplicationCore.h"
#include "pqDisplayPolicy.h"
#include "pqObjectBuilder.h"
#include "pqOutputPort.h"
#include "pqPipelineFilter.h"
#include "pqPipelineSource.h"
#include "pqRenderView.h"
#include "pqScalarsToColors.h"
#include "pqServer.h"
#include "pqServerManagerModel.h"
#include "pqSettings.h"
#include "pqSMAdaptor.h"
#include "pqUndoStack.h"

//-----------------------------------------------------------------------------
class pqPipelineRepresentation::pqInternal
{
public:
  vtkSmartPointer<vtkSMRepresentationProxy> RepresentationProxy;
  vtkSmartPointer<vtkEventQtSlotConnect> VTKConnect;

  pqInternal()
    {
    this->VTKConnect = vtkSmartPointer<vtkEventQtSlotConnect>::New();
    }

  static vtkPVArrayInformation* getArrayInformation(const pqPipelineRepresentation* repr,
    const char* arrayname, int fieldType)
    {
    if (!arrayname || !arrayname[0] || !repr)
      {
      return NULL;
      }

    vtkPVDataInformation* dataInformation = repr->getInputDataInformation();
    vtkPVArrayInformation* arrayInfo = NULL;
    if (dataInformation)
      {
      arrayInfo = dataInformation->GetAttributeInformation(fieldType)->
        GetArrayInformation(arrayname);
      }
    if (!arrayInfo)
      {
      dataInformation = repr->getRepresentedDataInformation();
      if (dataInformation)
        {
        arrayInfo = dataInformation->GetAttributeInformation(fieldType)->
          GetArrayInformation(arrayname);
        }
      }
    return arrayInfo;
    }

};

//-----------------------------------------------------------------------------
pqPipelineRepresentation::pqPipelineRepresentation(
  const QString& group,
  const QString& name,
  vtkSMProxy* display,
  pqServer* server, QObject* p/*=null*/):
  Superclass(group, name, display, server, p)
{
  this->Internal = new pqPipelineRepresentation::pqInternal();
  this->Internal->RepresentationProxy
    = vtkSMRepresentationProxy::SafeDownCast(display);

  if (!this->Internal->RepresentationProxy)
    {
    qFatal("Display given is not a vtkSMRepresentationProxy.");
    }

  /*
  // Whenever representation changes to VolumeRendering, we have to
  // ensure that the ColorArray has been initialized to something.
  // Otherwise, the VolumeMapper segfaults.
  this->Internal->VTKConnect->Connect(
    display->GetProperty("Representation"), vtkCommand::ModifiedEvent,
    this, SLOT(onRepresentationChanged()), 0, 0, Qt::QueuedConnection);
    */

  QObject::connect(this, SIGNAL(visibilityChanged(bool)),
    this, SLOT(updateScalarBarVisibility(bool)));

  // Whenever the pipeline gets be updated, it's possible that the scalar ranges
  // change. If that happens, we try to ensure that the lookuptable range is big
  // enough to show the entire data (unless of course, the user locked the
  // lookuptable ranges).
  this->Internal->VTKConnect->Connect(
    display, vtkCommand::UpdateDataEvent,
    this, SLOT(onDataUpdated()));
  this->UpdateLUTRangesOnDataUpdate = true;
}

//-----------------------------------------------------------------------------
pqPipelineRepresentation::~pqPipelineRepresentation()
{
  delete this->Internal;
}

//-----------------------------------------------------------------------------
vtkSMRepresentationProxy* pqPipelineRepresentation::getRepresentationProxy() const
{
  return this->Internal->RepresentationProxy;
}

//-----------------------------------------------------------------------------
vtkSMProxy* pqPipelineRepresentation::getScalarOpacityFunctionProxy()
{
  // We may want to create a new proxy is none exists.
  return pqSMAdaptor::getProxyProperty(
    this->getProxy()->GetProperty("ScalarOpacityFunction"));
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::onInputChanged()
{
  if (this->getInput())
    {
    QObject::disconnect(this->getInput(),
      SIGNAL(modifiedStateChanged(pqServerManagerModelItem*)),
      this, SLOT(onInputAccepted()));
    }

  this->Superclass::onInputChanged();

  if (this->getInput())
    {
    /// We need to try to update the LUT ranges only when the user manually
    /// changed the input source object (not necessarily ever pipeline update
    /// which can happen when time changes -- for example). So we listen to this
    /// signal. BUG #10062.
    QObject::connect(this->getInput(),
      SIGNAL(modifiedStateChanged(pqServerManagerModelItem*)),
      this, SLOT(onInputAccepted()));
    }
}

//-----------------------------------------------------------------------------
int pqPipelineRepresentation::getNumberOfComponents(
  const char* arrayname, int fieldtype)
{
  vtkPVArrayInformation* info = pqInternal::getArrayInformation(this,
    arrayname, fieldtype);
  return (info? info->GetNumberOfComponents() : 0);
}
//-----------------------------------------------------------------------------
QString pqPipelineRepresentation::getComponentName(
  const char* arrayname, int fieldtype, int component)
{
  vtkPVArrayInformation* info = pqInternal::getArrayInformation(this,
    arrayname, fieldtype);

   if ( info )
     {
     return QString (info->GetComponentName( component ) );     
     }  

   //failed to find info, return empty string
  return QString();
}

//-----------------------------------------------------------------------------
QString pqPipelineRepresentation::getRepresentationType() const
{
  vtkSMProxy* repr = this->getRepresentationProxy();
  if (repr && repr->GetProperty("Representation"))
    {
    // this handles enumeration domains as well.
    return vtkSMPropertyHelper(repr, "Representation").GetAsString(0);
    }

  const char* xmlname = repr->GetXMLName();
  if (strcmp(xmlname, "OutlineRepresentation") == 0)
    {
    return "Outline";
    }

  if (strcmp(xmlname, "UnstructuredGridVolumeRepresentation") == 0 ||
    strcmp(xmlname, "UniformGridVolumeRepresentation") == 0)
    {
    return "Volume";
    }

  if (strcmp(xmlname, "ImageSliceRepresentation") == 0)
    {
    return "Slice";
    }

  qCritical() << "pqPipelineRepresentation created for a incorrect proxy : " << xmlname;
  return 0;
}

//-----------------------------------------------------------------------------
double pqPipelineRepresentation::getOpacity() const
{
  vtkSMProperty* prop = this->getProxy()->GetProperty("Opacity");
  return (prop? pqSMAdaptor::getElementProperty(prop).toDouble() : 1.0);
}
//-----------------------------------------------------------------------------
void pqPipelineRepresentation::setColor(double R,double G,double B)
{
  double rgb[] = {R,G,B};
  vtkSMPropertyHelper(this->getProxy()->GetProperty("Color")).Set(rgb, 3);
  this->getProxy()->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::resetLookupTableScalarRange()
{
  vtkSMProxy* proxy = this->getProxy();
  if (vtkSMPVRepresentationProxy::GetUsingScalarColoring(proxy))
    {
    vtkSMPVRepresentationProxy::RescaleTransferFunctionToDataRange(proxy);
    }
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::resetLookupTableScalarRangeOverTime()
{
  vtkSMProxy* proxy = this->getProxy();
  if (vtkSMPVRepresentationProxy::GetUsingScalarColoring(proxy))
    {
    vtkSMPVRepresentationProxy::RescaleTransferFunctionToDataRangeOverTime(proxy);
    }
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::onInputAccepted()
{
  // BUG #10062
  // This slot gets called when the input to the representation is "accepted".
  // We mark this representation's LUT ranges dirty so that when the pipeline
  // finally updates, we can reset the LUT ranges.
  if (this->getInput()->modifiedState() == pqProxy::MODIFIED)
    {
    this->UpdateLUTRangesOnDataUpdate = true;
    }
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::onDataUpdated()
{
  if (this->UpdateLUTRangesOnDataUpdate ||
    pqScalarsToColors::colorRangeScalingMode() ==
    pqScalarsToColors::GROW_ON_UPDATED)
    {
    // BUG #10062
    // Since this part of the code happens every time the pipeline is updated, we
    // don't need to record it on the undo stack. It will happen automatically
    // each time.
    BEGIN_UNDO_EXCLUDE();
    this->UpdateLUTRangesOnDataUpdate = false;
    this->updateLookupTableScalarRange();
    END_UNDO_EXCLUDE();
    }
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::updateLookupTableScalarRange()
{
  pqScalarsToColors* lut = this->getLookupTable();
  if (!lut || lut->getScalarRangeLock())
    {
    return;
    }

  vtkSMProxy* proxy = this->getProxy();
  if (vtkSMPVRepresentationProxy::GetUsingScalarColoring(proxy))
    {
    // if scale range was initialized, we extend it, other we simply reset it.
    vtkSMPropertyHelper helper(lut->getProxy(), "ScalarRangeInitialized", true);
    bool extend_lut = helper.GetAsInt() == 1;
    vtkSMPVRepresentationProxy::RescaleTransferFunctionToDataRange(proxy,
      extend_lut);
    // mark the scalar range as initialized.
    helper.Set(0, 1);
    lut->getProxy()->UpdateVTKObjects();
    }
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::setRepresentation(const QString& representation)
{
  vtkSMRepresentationProxy* repr = this->getRepresentationProxy();
  vtkSMPropertyHelper(repr, "Representation").Set(representation.toLatin1().data());
  repr->UpdateVTKObjects();
  this->onRepresentationChanged();
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::onRepresentationChanged()
{
  cout << "FIXME" << __FILE__ << ":" << __LINE__ << endl;
  // FIXME:
  //vtkSMRepresentationProxy* repr = this->getRepresentationProxy();
  //if (!repr)
  //  {
  //  return;
  //  }

  //QString reprType = this->getRepresentationType();
  //if (reprType.compare("Volume", Qt::CaseInsensitive) != 0 &&
  //  reprType.compare("Slice", Qt::CaseInsensitive) != 0)
  //  {
  //  // Nothing to do here.
  //  return;
  //  }

  //// Representation is Volume, is color array set?
  //QList<QString> colorFields = this->getColorFields();
  //if (colorFields.size() == 0)
  //  {
  //  qCritical() << 
  //    "Cannot volume render since no point (or cell) data available.";
  //  this->setRepresentation("Outline");
  //  return;
  //  }

  //QString colorField = this->getColorField(false);
  //if(!colorFields.contains(colorField))
  //  {
  //  // Current color field is not suitable for Volume rendering.
  //  // Change it.
  //  this->setColorField(colorFields[0]);
  //  }

  //this->updateLookupTableScalarRange();
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::updateScalarBarVisibility(bool visible)
{
  qDebug("FIXME");
}
//-----------------------------------------------------------------------------
const char* pqPipelineRepresentation::UNSTRUCTURED_GRID_OUTLINE_THRESHOLD()
{
  // BUG #13564. I am renaming this key since we want to change the default
  // value for this key. Renaming the key makes it possible to override the
  // value without forcing users to reset their .config files.
  return "/representation/UnstructuredGridOutlineThreshold2";
}

//-----------------------------------------------------------------------------
void pqPipelineRepresentation::setUnstructuredGridOutlineThreshold(double numcells)
{
  pqApplicationCore* core = pqApplicationCore::instance();
  pqSettings* settings = core->settings();
  if (settings)
    {
    settings->setValue(
      pqPipelineRepresentation::UNSTRUCTURED_GRID_OUTLINE_THRESHOLD(), 
      QVariant(numcells));
    }
}

//-----------------------------------------------------------------------------
double pqPipelineRepresentation::getUnstructuredGridOutlineThreshold()
{
  pqApplicationCore* core = pqApplicationCore::instance();
  pqSettings* settings = core->settings();
  if (settings && settings->contains(
      pqPipelineRepresentation::UNSTRUCTURED_GRID_OUTLINE_THRESHOLD()))
    {
    bool ok;
    double numcells = settings->value(
      pqPipelineRepresentation::UNSTRUCTURED_GRID_OUTLINE_THRESHOLD()).toDouble(&ok);
    if (ok)
      {
      return numcells;
      }
    }

  return 250; //  250 million cells.
}

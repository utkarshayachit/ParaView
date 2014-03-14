/*=========================================================================

   Program: ParaView
   Module:    pqDisplayColorWidget.cxx

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
#include "pqDisplayColorWidget.h"

#include "pqDataRepresentation.h"
#include "pqPropertiesPanel.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqUndoStack.h"
#include "vtkDataObject.h"
#include "vtkEventQtSlotConnect.h"
#include "vtkNew.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkSMArrayListDomain.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkWeakPointer.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QtDebug>


/// This class makes it possible to add custom logic when updating the
/// "ColorArrayName" property instead of directly setting the SMProperty. The
/// custom logic, in this case, ensures that the LUT is setup and initialized
/// (all done by vtkSMPVRepresentationProxy::SetScalarColoring()).
class pqDisplayColorWidget::PropertyLinksConnection : public pqPropertyLinksConnection
{
  typedef pqPropertyLinksConnection Superclass;
  typedef pqDisplayColorWidget::ValueType ValueType;
public:
  PropertyLinksConnection(
    QObject* qobject, const char* qproperty, const char* qsignal,
    vtkSMProxy* smproxy, vtkSMProperty* smproperty, int smindex,
    bool use_unchecked_modified_event,
    QObject* parentObject=0)
    : Superclass(qobject, qproperty, qsignal,
      smproxy, smproperty, smindex, use_unchecked_modified_event,
      parentObject)
    {
    }
  virtual ~PropertyLinksConnection()
    {
    }

  // Methods to convert between ValueType and QVariant. Since QVariant doesn't
  // support == operations of non-default Qt types, we are forced to convert the
  // ValueType to a QStringList.
  static ValueType convert(const QVariant& value)
    {
    if (!value.isValid()) { return ValueType(); }
    Q_ASSERT(value.canConvert<QStringList>());
    QStringList strList = value.toStringList();
    Q_ASSERT(strList.size() == 2);
    return ValueType(strList[0].toInt(), strList[1]);
    }
  static QVariant convert(const ValueType& value)
    {
    if (value.first < vtkDataObject::POINT || value.first > vtkDataObject::CELL ||
      value.second.isEmpty())
      {
      return QVariant();
      }
    QStringList val;
    val << QString::number(value.first)
        << value.second;
    return val;
    }

protected:
  /// Called to update the ServerManager Property due to UI change.
  virtual void setServerManagerValue(bool use_unchecked, const QVariant& value)
    {
    Q_ASSERT(use_unchecked == false);

    ValueType val = this->convert(value);
    int association = val.first;
    const QString &arrayName = val.second;

    BEGIN_UNDO_SET("Change coloring");
    vtkSMProxy* reprProxy = this->proxySM();
    if (vtkSMPVRepresentationProxy::SetScalarColoring(
        reprProxy, arrayName.toAscii().data(), association))
      {
      // we could now respect some application setting to determine if the LUT is
      // to be reset.
      vtkSMPVRepresentationProxy::RescaleTransferFunctionToDataRange(reprProxy, true);
      }
    END_UNDO_SET();
    }

  /// called to get the current value for the ServerManager Property.
  virtual QVariant currentServerManagerValue(bool use_unchecked) const
    {
    Q_ASSERT(use_unchecked == false);
    ValueType val;
    vtkSMProxy* reprProxy = this->proxySM();
    if (vtkSMPVRepresentationProxy::GetUsingScalarColoring(reprProxy))
      {
      vtkSMPropertyHelper helper(this->propertySM());
      val.first = helper.GetInputArrayAssociation();
      val.second = helper.GetInputArrayNameToProcess();
      }
    return this->convert(val);
    }

  /// called to set the UI due to a ServerManager Property change.
  virtual void setQtValue(const QVariant& value)
    {
    ValueType val = this->convert(value);
    qobject_cast<pqDisplayColorWidget*>(this->objectQt())->setArraySelection(val);
    }

  /// called to get the UI value.
  virtual QVariant currentQtValue() const
    {
    ValueType curVal = qobject_cast<pqDisplayColorWidget*>(
      this->objectQt())->arraySelection();
    return this->convert(curVal);
    }
private:
  Q_DISABLE_COPY(PropertyLinksConnection);
};

class pqDisplayColorWidget::pqInternals
{
public:
  pqPropertyLinks Links;
  vtkNew<vtkEventQtSlotConnect> Connector;
  vtkWeakPointer<vtkSMArrayListDomain> Domain;
};

//-----------------------------------------------------------------------------
pqDisplayColorWidget::pqDisplayColorWidget( QWidget *parentObject ) :
  Superclass(parentObject),
  CachedRepresentation(NULL),
  Internals(new pqDisplayColorWidget::pqInternals())
{
  this->CellDataIcon = new QIcon(":/pqWidgets/Icons/pqCellData16.png");
  this->PointDataIcon = new QIcon(":/pqWidgets/Icons/pqPointData16.png");
  this->SolidColorIcon = new QIcon(":/pqWidgets/Icons/pqSolidColor16.png");

  QHBoxLayout* hbox = new QHBoxLayout(this);
  hbox->setMargin(0);
  hbox->setSpacing(pqPropertiesPanel::suggestedHorizontalSpacing());

  this->Variables = new QComboBox( this );
  this->Variables->setMaxVisibleItems(60);
  this->Variables->setObjectName("Variables");
  this->Variables->setMinimumSize( QSize( 150, 0 ) );
  this->Variables->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  this->Components = new QComboBox( this );
  this->Components->setObjectName("Components");

  hbox->addWidget(this->Variables);
  hbox->addWidget(this->Components);

  this->Variables->setEnabled(false);
  this->Components->setEnabled(false);
}

//-----------------------------------------------------------------------------
pqDisplayColorWidget::~pqDisplayColorWidget()
{
  delete this->CellDataIcon;
  delete this->PointDataIcon;
  delete this->SolidColorIcon;
  delete this->Internals;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::renderActiveView()
{
  if (this->Representation)
    {
    this->Representation->renderViewEventually();
    }
}
//-----------------------------------------------------------------------------
void pqDisplayColorWidget::setRepresentation(pqDataRepresentation* repr)
{
  if (this->CachedRepresentation == repr)
    {
    return;
    }

  if (this->Representation)
    {
    this->disconnect(this->Representation);
    this->Representation = NULL;
    }

  this->Internals->Links.clear();
  this->Internals->Connector->Disconnect();
  this->Internals->Domain = NULL;
  this->Variables->clear();
  this->Components->clear();

  // now, save the new repr.
  this->CachedRepresentation = repr;
  this->Representation = repr;

  vtkSMProxy* proxy = repr? repr->getProxy() : NULL;
  bool can_color = (proxy != NULL && proxy->GetProperty("ColorArrayName") != NULL);
  if (!can_color)
    {
    this->Variables->setEnabled(false);
    this->Components->setEnabled(false);
    return;
    }

  // monitor LUT changes. we need to link the component number on the LUT's
  // property.
  this->connect(repr, SIGNAL(colorTransferFunctionModified()),
    SLOT(updateColorTransferFunction()));

  vtkSMProperty* prop = proxy->GetProperty("ColorArrayName");
  vtkSMArrayListDomain* domain = vtkSMArrayListDomain::SafeDownCast(
    prop->FindDomain("vtkSMArrayListDomain"));
  if (!domain)
    {
    qCritical("Representation has ColorArrayName property without "
      "a vtkSMArrayListDomain. This is no longer supported.");
    return;
    }
  this->Internals->Domain = domain;

  this->Internals->Connector->Connect(
    prop, vtkCommand::DomainModifiedEvent, this, SLOT(refreshColorArrayNames()));
  this->refreshColorArrayNames();

  this->Internals->Links.addPropertyLink<PropertyLinksConnection>(
    this, "notused", SIGNAL(arraySelectionChanged()), proxy, prop);

  this->Variables->setEnabled(true);
  this->Components->setEnabled(true);

  this->connect(this->Variables,
    SIGNAL(currentIndexChanged(int)), SIGNAL(arraySelectionChanged()));
  this->connect(this->Variables,
    SIGNAL(currentIndexChanged(int)), SLOT(refreshComponents()));

  this->connect(&this->Internals->Links,
    SIGNAL(qtWidgetChanged()), SLOT(renderActiveView()));
}

//-----------------------------------------------------------------------------
QPair<int, QString> pqDisplayColorWidget::arraySelection() const
{
  if (this->Variables->currentIndex() != -1)
    {
    return PropertyLinksConnection::convert(this->Variables->itemData(
        this->Variables->currentIndex()));
    }

  return QPair<int, QString>();
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::setArraySelection(
  const QPair<int, QString> & value)
{
  int association = value.first;
  const QString& arrayName = value.second;

  if (association < vtkDataObject::POINT || association > vtkDataObject::CELL)
    {
    qCritical("Only cell/point data coloring is currently supported by this widget.");
    association = vtkDataObject::POINT;
    }

  QVariant data = this->itemData(association, arrayName);
  QIcon* icon = this->itemIcon(association, arrayName);

  int index = this->Variables->findData(data);
  if (index == -1)
    {
    bool prev = this->Variables->blockSignals(true);
    this->Variables->addItem(*icon, arrayName, data);
    this->Variables->blockSignals(prev);

    index = this->Variables->findData(data);
    qDebug() << "(" << association << ", " << arrayName << " ) "
      "is not an array shown in the pqDisplayColorWidget currently. "
      "Will add a new entry for it";
    }
  Q_ASSERT(index != -1);
  this->Variables->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------
QVariant pqDisplayColorWidget::itemData(int association, const QString& arrayName) const
{
  return PropertyLinksConnection::convert(ValueType(association, arrayName));
}

//-----------------------------------------------------------------------------
QIcon* pqDisplayColorWidget::itemIcon(int association, const QString& arrayName) const
{
  QVariant data = this->itemData(association, arrayName);
  if (!data.isValid())
    {
    return this->SolidColorIcon;
    }
  return association == vtkDataObject::CELL?  this->CellDataIcon: this->PointDataIcon;
}

//-----------------------------------------------------------------------------
int pqDisplayColorWidget::componentNumber() const
{
  // here, we treat -1 as the magnitude (to be consistent with
  // VTK/vtkScalarsToColors). This mismatches with how
  // vtkPVArrayInformation refers to magnitude.
  int index = this->Components->currentIndex();
  return index >=0? this->Components->itemData(index).toInt() : -1;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::setComponentNumber(int val)
{
  int index = this->Components->findData(val);
  if (index == -1)
    {
    bool prev = this->Components->blockSignals(true);
    this->Components->addItem(QString("%1").arg(val), val);
    this->Components->blockSignals(false);

    index = this->Components->findData(val);
    qDebug() << "Component " << val << " is not currently known. "
      "Will add a new entry for it.";
    }
  Q_ASSERT(index != -1);
  this->Components->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::refreshColorArrayNames()
{
  // Simply update the this->Variables combo-box with values from the domain.
  // Try to preserve the current text if possible.
  QVariant current = PropertyLinksConnection::convert(this->arraySelection());

  bool prev = this->Variables->blockSignals(true);

  this->Variables->clear();

  // add solid color.
  this->Variables->addItem(*this->SolidColorIcon, "Solid Color", QVariant());

  vtkSMArrayListDomain* domain = this->Internals->Domain;
  Q_ASSERT (domain);

  for (unsigned int cc=0, max = domain->GetNumberOfStrings(); cc < max; cc++)
    {
    int icon_association = domain->GetFieldAssociation(cc);
    int association = domain->GetDomainAssociation(cc);
    QString name = domain->GetString(cc);

    QIcon* icon = this->itemIcon(icon_association, name);
    QVariant data = this->itemData(association, name);
    this->Variables->addItem(*icon, name, data);
    }
  int idx = this->Variables->findData(current);
  if (idx >= 0)
    {
    this->Variables->setCurrentIndex(idx);
    }
  this->Variables->blockSignals(prev);
  this->refreshComponents();
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::refreshComponents()
{
  if (!this->Representation)
    {
    return;
    }

  // try to preserve the component number.
  int current = this->componentNumber();

  QPair<int, QString> val = this->arraySelection();
  int association = val.first;
  const QString &arrayName = val.second;

  vtkPVDataInformation* dataInfo = this->Representation->getInputDataInformation();
  vtkPVArrayInformation* arrayInfo = dataInfo?
    dataInfo->GetArrayInformation(arrayName.toAscii().data(), association) : NULL;
  if (!arrayInfo)
    {
    vtkPVDataInformation* reprInfo = this->Representation->getRepresentedDataInformation();
    arrayInfo = reprInfo?
      reprInfo->GetArrayInformation(arrayName.toAscii().data(), association) : NULL;
    }

  if (!arrayInfo || arrayInfo->GetNumberOfComponents() <= 1)
    {
    bool prev = this->Components->blockSignals(true);
    this->Components->clear();
    this->Components->blockSignals(prev);
    return;
    }

  bool prev = this->Components->blockSignals(true);

  this->Components->clear();
  for (int cc=0, max=arrayInfo->GetNumberOfComponents(); cc < max; cc++)
    {
    if (cc==0 && max > 1)
      {
      // add magnitude for non-scalar values.
      this->Components->addItem(arrayInfo->GetComponentName(-1), -1);
      }
    this->Components->addItem(arrayInfo->GetComponentName(cc), cc);
    }

  /// restore component choice if possible.
  int idx = this->Components->findData(current);
  if (idx >= 0)
    {
    this->Components->setCurrentIndex(idx);
    }
  this->Components->blockSignals(prev);
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::updateColorTransferFunction()
{
}

//
#if 0
//-----------------------------------------------------------------------------
void pqDisplayColorWidget::clear()
{
  this->BlockEmission++;
  this->Variables->clear();
  this->BlockEmission--;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::addVariable(pqVariableType type, 
  const QString& arg_name, bool is_partial)
{
  QString name = arg_name;
  if (is_partial)
    {
    name += " (partial)";
    }

  // Don't allow duplicates to creep in ...
  if(-1 != this->Variables->findData(this->variableData(type, arg_name)))
    {
    return;
    }

  this->BlockEmission++;
  switch(type)
    {
    case VARIABLE_TYPE_NONE:
      this->Variables->addItem(*this->SolidColorIcon,
        "Solid Color", this->variableData(type, arg_name));
      break;

    case VARIABLE_TYPE_NODE:
      this->Variables->addItem(*this->PointDataIcon, name, 
        this->variableData(type, arg_name));
      break;

    case VARIABLE_TYPE_CELL:
      this->Variables->addItem(*this->CellDataIcon,
        name, this->variableData(type, arg_name));
      break;
    }
  this->BlockEmission--;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::chooseVariable(pqVariableType type, 
  const QString& name)
{
  const int row = this->Variables->findData(variableData(type, name));
  if(row != -1)
    {
    this->Variables->setCurrentIndex(row);
    }
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::onComponentActivated(int row)
{
  if(this->BlockEmission)
    {
    return;
    }
  
  pqPipelineRepresentation* display = this->getRepresentation();
  if(display)
    {
    BEGIN_UNDO_SET("Color Component Change");
    pqScalarsToColors* lut = display->getLookupTable();
    if(row == 0)
      {
      lut->setVectorMode(pqScalarsToColors::MAGNITUDE, -1);
      }
    else
      {
      lut->setVectorMode(pqScalarsToColors::COMPONENT, row-1);
      }
    lut->updateScalarBarTitles(this->Components->itemText(row));
    display->resetLookupTableScalarRange();
    END_UNDO_SET();

    display->renderViewEventually();
    }
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::onVariableActivated(int row)
{
  if(this->BlockEmission)
    {
    return;
    }

  const QStringList d = this->Variables->itemData(row).toStringList();
  if(d.size() != 2)
    return;
    
  pqVariableType type = VARIABLE_TYPE_NONE;
  if(d[1] == "cell")
    {
    type = VARIABLE_TYPE_CELL;
    }
  else if(d[1] == "point")
    {
    type = VARIABLE_TYPE_NODE;
    }
    
  const QString name = d[0];
  
  emit variableChanged(type, name);
  emit this->modified();
}

//-----------------------------------------------------------------------------
const QStringList pqDisplayColorWidget::variableData(pqVariableType type, 
                                                     const QString& name)
{
  QStringList result;
  result << name;

  switch(type)
    {
    case VARIABLE_TYPE_NONE:
      result << "none";
      break;
    case VARIABLE_TYPE_NODE:
      result << "point";
      break;
    case VARIABLE_TYPE_CELL:
      result << "cell";
      break;
    default:
      // Return empty list.
      return QStringList();
    }
    
  return result;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::onVariableChanged(pqVariableType type, 
  const QString& name)
{
  pqPipelineRepresentation* display = this->getRepresentation();
  if (display)
    {
    BEGIN_UNDO_SET("Color Change");
    switch(type)
      {
    case VARIABLE_TYPE_NONE:
      display->colorByArray(NULL, 0);
      break;
    case VARIABLE_TYPE_NODE:
      display->colorByArray(name.toLatin1().data(),
        vtkDataObject::FIELD_ASSOCIATION_POINTS);
      break;
    case VARIABLE_TYPE_CELL:
      display->colorByArray(name.toLatin1().data(),
        vtkDataObject::FIELD_ASSOCIATION_CELLS);
      break;
      }
    END_UNDO_SET();
    display->renderViewEventually();
    }
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::updateGUI()
{
  this->BlockEmission++;
  pqPipelineRepresentation* display = this->getRepresentation();
  if (display)
    {
    int index = this->AvailableArrays.indexOf(display->getColorField());
    if (index < 0)
      {
      index = 0;
      }
    this->Variables->setCurrentIndex(index);
    this->updateComponents();
    }
  this->BlockEmission--;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::updateComponents()
{
  this->BlockEmission++;
  this->Components->clear();

  pqPipelineRepresentation* display = this->getRepresentation();
  if (display)
    {
    pqScalarsToColors* lut = display->getLookupTable();
    int numComponents = display->getColorFieldNumberOfComponents(
        display->getColorField());
    if ( lut && numComponents == 1 )
      {
      //we need to show a single name
      QString compName = display->getColorFieldComponentName( 
        display->getColorField(), 0);
      if ( compName.length() > 0 )
        {
        //only add an item if a component name exists, that way
        //we don't get a drop down box with a single empty item
        this->Components->addItem( compName );          
        }
      }
    else if(lut && numComponents > 1)
      {
      // delayed connection for when lut finally exists
      // remove previous connection, if any
      this->VTKConnect->Disconnect(
        lut->getProxy(), vtkCommand::PropertyModifiedEvent, 
        this, SLOT(updateGUI()), NULL);
      this->VTKConnect->Connect(
        lut->getProxy(), vtkCommand::PropertyModifiedEvent, 
        this, SLOT(updateGUI()), NULL, 0.0);

      this->Components->addItem("Magnitude");
      for(int i=0; i<numComponents; i++)
        {        
        this->Components->addItem(  
          display->getColorFieldComponentName( display->getColorField(), i) );          
        }
      
      if(lut->getVectorMode() == pqScalarsToColors::MAGNITUDE)
        {
        this->Components->setCurrentIndex(0);
        }
      else
        {
        this->Components->setCurrentIndex(lut->getVectorComponent()+1);
        }
      }
    }
  this->BlockEmission--;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::setRepresentation(pqDataRepresentation* display)
{
  if (display == this->CachedRepresentation)
    {
    return;
    }

  if (this->Representation)
    {
    QObject::disconnect(this->Representation, 0, this, 0);
    }

  this->VTKConnect->Disconnect();
  this->Representation = qobject_cast<pqPipelineRepresentation*>(display);
  this->CachedRepresentation = display;
  if(this->Representation)
    {
    vtkSMProxy* repr = this->Representation->getProxy();
    this->VTKConnect->Connect(repr->GetProperty("ColorAttributeType"),
      vtkCommand::ModifiedEvent, this, SLOT(updateGUI()),
      NULL, 0.0);
    this->VTKConnect->Connect(repr->GetProperty("ColorArrayName"),
      vtkCommand::ModifiedEvent, this, SLOT(updateGUI()),
      NULL, 0.0);

    if (repr->GetProperty("Representation"))
      {
      this->VTKConnect->Connect(
        repr->GetProperty("Representation"), vtkCommand::ModifiedEvent,
        this, SLOT(updateGUI()),
        NULL, 0.0);
      }

    // Every time the display updates, it is possible that the arrays available for
    // coloring have changed, hence we reload the list.
    QObject::connect(this->Representation, SIGNAL(dataUpdated()),
      this, SLOT(reloadGUI()));
    }
  this->reloadGUI();
}

//-----------------------------------------------------------------------------
pqPipelineRepresentation* pqDisplayColorWidget::getRepresentation() const
{
  return this->Representation;
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::reloadGUI()
{
  this->reloadGUIInternal();
}

//-----------------------------------------------------------------------------
void pqDisplayColorWidget::reloadGUIInternal()
{
  this->Updating = false;
  this->BlockEmission++;
  this->clear();

  pqPipelineRepresentation* display = this->getRepresentation();
  if (!display)
    {
    this->addVariable(VARIABLE_TYPE_NONE, "Solid Color", false);
    this->BlockEmission--;
    this->setEnabled(false);
    return;
    }
  this->setEnabled(true);

  this->AvailableArrays = display->getColorFields();
  QRegExp regExpCell(" \\(cell\\)\\w*$");
  QRegExp regExpPoint(" \\(point\\)\\w*$");
  foreach(QString arrayName, this->AvailableArrays)
    {
    if (arrayName == "Solid Color")
      {
      this->addVariable(VARIABLE_TYPE_NONE, arrayName, false);
      }
    else if (regExpCell.indexIn(arrayName) != -1)
      {
      arrayName = arrayName.replace(regExpCell, "");
      this->addVariable(VARIABLE_TYPE_CELL, arrayName, 
        display->isPartial(arrayName, vtkDataObject::FIELD_ASSOCIATION_CELLS));
      }
    else if (regExpPoint.indexIn(arrayName) != -1)
      {
      arrayName = arrayName.replace(regExpPoint, "");
      this->addVariable(VARIABLE_TYPE_NODE, arrayName,
        display->isPartial(arrayName, vtkDataObject::FIELD_ASSOCIATION_POINTS));
      }
    }
    
  this->BlockEmission--;
  this->updateGUI();

  emit this->modified();
}
#endif

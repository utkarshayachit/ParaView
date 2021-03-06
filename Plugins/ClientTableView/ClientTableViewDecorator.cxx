#include "ClientTableViewDecorator.h"
#include "ClientTableView.h"
#include "ui_ClientTableViewDecorator.h"

#include "pqApplicationCore.h"
#include "pqComboBoxDomain.h"
#include "pqDataRepresentation.h"
#include "pqDisplayPolicy.h"
#include "pqMultiViewFrame.h"
#include "pqNamedWidgets.h"
#include "pqOutputPort.h"
#include "pqPipelineSource.h"
#include "pqPropertyHelper.h"
#include "pqPropertyLinks.h"
#include "pqPropertyManager.h"
#include "pqSignalAdaptors.h"
#include "pqView.h"

#include "vtkSMProxy.h"
#include "vtkSMProperty.h"

#include <QAction>
#include <QComboBox>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>



class ClientTableViewDecorator::pqInternal : public Ui::ClientTableViewDecorator
{
public:
  pqPropertyLinks Links;
  pqPropertyManager PropertyManager;  
  QPointer<pqSignalAdaptorComboBox> ArrayAdaptor;
  QPointer<pqComboBoxDomain> ArrayDomain;
  QPointer<pqSignalAdaptorComboBox> AttributeAdaptor; // Because these objects are parented
  QPointer<pqComboBoxDomain> AttributeDomain;         // by Qt it may get deleted so
                                                      // we are guarding it with a 
                                                      // QPointer so that we can check
                                                      // if it's been deleted later.

};

//-----------------------------------------------------------------------------
ClientTableViewDecorator::ClientTableViewDecorator(ClientTableView* view):
  Superclass(view)
{
  this->View = view;
  QWidget* container = view->getWidget();

  QWidget* header = new QWidget(container);
  QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(container->layout());
  
  this->Internal = new pqInternal();
  this->Internal->setupUi(header);
  //this->Internal->Source->setAutoUpdateIndex(false);
  //this->Internal->Source->addCustomEntry("None", NULL);
  //this->Internal->Source->fillExistingPorts();

  this->Internal->Source->hide();
  this->Internal->sourceLabel->hide();
  this->Internal->attribute_scalars->setEnabled(false);

  this->Internal->AttributeAdaptor = 0;
  this->Internal->ArrayAdaptor = 0;
  this->Internal->AttributeDomain = 0;
  this->Internal->ArrayDomain = 0;

  QObject::connect(&this->Internal->Links, SIGNAL(smPropertyChanged()),
    this->View, SLOT(render()));

  //QObject::connect(this->Internal->Source, SIGNAL(currentIndexChanged(pqOutputPort*)),
  //  this, SLOT(currentIndexChanged(pqOutputPort*)));
  QObject::connect(this->View, SIGNAL(showing(pqDataRepresentation*)),
    this, SLOT(showing(pqDataRepresentation*)));
  QObject::connect(this->Internal->allAttributes, SIGNAL(toggled(bool)),
    this, SLOT(setShowAllColumns(bool)));

  layout->insertWidget(0, header);
  this->showing(0); //TODO: get the actual repr currently shown by the view.
}

//-----------------------------------------------------------------------------
ClientTableViewDecorator::~ClientTableViewDecorator()
{
  delete this->Internal;
  this->Internal = 0;
}

//-----------------------------------------------------------------------------
void ClientTableViewDecorator::setShowAllColumns(bool state)
{
  QList<pqRepresentation*> reprs = this->View->getRepresentations();
  foreach (pqRepresentation* repr, reprs)
    {
    if (repr->isVisible())
      {
      vtkSMProxy* proxy = repr->getProxy();
      pqPropertyHelper(proxy, "AllColumns").Set(state);
      proxy->UpdateVTKObjects();
      break;
      }
    }
}

//-----------------------------------------------------------------------------
void ClientTableViewDecorator::showing(pqDataRepresentation* repr)
{
  this->Internal->Links.removeAllPropertyLinks();
  delete this->Internal->AttributeDomain;
  delete this->Internal->AttributeAdaptor;
  delete this->Internal->ArrayDomain;
  delete this->Internal->ArrayAdaptor;
  this->Internal->AttributeDomain = 0;
  this->Internal->AttributeAdaptor = 0;
  this->Internal->ArrayDomain = 0;
  this->Internal->ArrayAdaptor = 0;
  if (repr)
    {
    vtkSMProxy* reprProxy = repr->getProxy();

    //this->Internal->Source->setCurrentPort(repr->getOutputPortFromInput());

    pqPipelineSource* input = repr->getInput();
    QObject::connect(input, SIGNAL(dataUpdated(pqPipelineSource*)), 
      this, SLOT(dataUpdated()));
    this->dataUpdated();

    this->Internal->AttributeDomain = new pqComboBoxDomain(
      this->Internal->attribute_mode,
      reprProxy->GetProperty("Attribute"), "field_list");
    this->Internal->AttributeDomain->setObjectName("FieldModeDomain");
    this->Internal->AttributeAdaptor = 
      new pqSignalAdaptorComboBox(this->Internal->attribute_mode);
    this->Internal->PropertyManager.registerLink(
      this->Internal->AttributeAdaptor, "currentText", 
      SIGNAL(currentTextChanged(const QString&)),
      reprProxy, reprProxy->GetProperty("Attribute"), 0);  
    this->Internal->Links.addPropertyLink(this->Internal->AttributeAdaptor,
      "currentText", SIGNAL(currentTextChanged(const QString&)),
      reprProxy, reprProxy->GetProperty("Attribute"),0);

    this->Internal->ArrayDomain = new pqComboBoxDomain(
      this->Internal->attribute_scalars,
      reprProxy->GetProperty("Attribute"), "array_list");
    this->Internal->ArrayDomain->setObjectName("FieldScalarsDomain");
    this->Internal->ArrayAdaptor = 
      new pqSignalAdaptorComboBox(this->Internal->attribute_scalars);
    this->Internal->PropertyManager.registerLink(
      this->Internal->ArrayAdaptor, "currentText", 
      SIGNAL(currentTextChanged(const QString&)),
      reprProxy, reprProxy->GetProperty("Attribute"), 1);  
    this->Internal->Links.addPropertyLink(this->Internal->ArrayAdaptor,
      "currentText", SIGNAL(currentTextChanged(const QString&)),
      reprProxy, reprProxy->GetProperty("Attribute"),1);

    this->Internal->Links.addPropertyLink(
      this->Internal->allAttributes,
      "checked",
      SIGNAL(toggled(bool)),
      reprProxy,
      reprProxy->GetProperty("AllColumns"));
    }
  else
    {
    //this->Internal->Source->setCurrentPort(NULL);
    }

  //this->Internal->attribute_mode->setEnabled(repr != 0);
  //this->Internal->attribute_scalars->setEnabled(repr != 0);
}

//-----------------------------------------------------------------------------
void ClientTableViewDecorator::currentIndexChanged(pqOutputPort* port)
{
  if (port)
    {
    pqDisplayPolicy* dpolicy = 
      pqApplicationCore::instance()->getDisplayPolicy();
    // Will create new display if needed. May also create new view 
    // as defined by the policy.
    if (dpolicy->setRepresentationVisibility(port, this->View, true))
      {
      this->View->render();
      }
    }
  else
    {
    QList<pqRepresentation*> reprs = this->View->getRepresentations();
    foreach (pqRepresentation* repr, reprs)
      {
      if (repr->isVisible())
        {
        repr->setVisible(false);
        this->View->render();
        break; // since only 1 repr can be visible at a time.
        }
      }
    }
}

void ClientTableViewDecorator::dataUpdated()
{
  QList<pqRepresentation*> reprs = this->View->getRepresentations();
  foreach (pqRepresentation* repr, reprs)
    {
    if (repr->isVisible())
      {
      vtkSMProxy* proxy = repr->getProxy();
      vtkSMProperty* prop = proxy->GetProperty("Input");
      if (prop)
        {
        prop->UpdateDependentDomains();
        }
      break; // since only 1 repr can be visible at a time.
      }
    }
}

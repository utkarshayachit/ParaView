

#include "pqIntRangeWidget.h"
#include "pqPropertyLinks.h"
#include "vtkCallbackCommand.h"
#include "vtkInitializationHelper.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkPVOptions.h"
#include "vtkPVRenderView.h"
#include "vtkPVRenderViewProxy.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkRenderView.h"
#include "vtkRenderWindow.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyManager.h"

#include <QApplication>
#include <QMainWindow>
#include "QVTKWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>

// returns sphere proxy
vtkSMProxy* addSphere(vtkSMProxy* view, vtkSMProxy* sphere = NULL)
{
  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();
  if (!sphere)
    {
    sphere  = pxm->NewProxy("sources", "SphereSource");
    sphere->SetConnectionID(view->GetConnectionID());
    sphere->UpdateVTKObjects();
    }
  else
    {
    sphere->Register(NULL);
    }

  vtkSMProxy* repr = pxm->NewProxy("new_representations",
    "GeometryRepresentation2");
  repr->SetConnectionID(view->GetConnectionID());
  vtkSMPropertyHelper(repr, "Input").Set(sphere);
  repr->UpdateVTKObjects();

  vtkSMPropertyHelper(view, "Representations").Add(repr);
  view->UpdateVTKObjects();

  sphere->Delete();
  repr->Delete();

  return sphere;
}

vtkSMProxy* createScalarBar(vtkSMProxy* view)
{
  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();
  vtkSMProxy* sb = pxm->NewProxy("representations",
    "ScalarBarWidgetRepresentation");
  sb->SetConnectionID(view->GetConnectionID());

  vtkSMProxy* lut = pxm->NewProxy("lookup_tables",
    "LookupTable");
  lut->SetConnectionID(view->GetConnectionID());
  lut->SetServers(vtkProcessModule::CLIENT_AND_SERVERS);
  lut->UpdateVTKObjects();

  vtkSMPropertyHelper(sb, "LookupTable").Set(lut);
  vtkSMPropertyHelper(sb, "Enabled").Set(1);
  sb->UpdateVTKObjects();
  lut->Delete();

  vtkSMPropertyHelper(view, "Representations").Add(sb);
  sb->Delete();
  return sb;
}

#define SECOND_WINDOW
#define REMOTE_CONNECTION_CS
////#define REMOTE_CONNECTION_CRS

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  QMainWindow mainWindow;
  mainWindow.resize(400, 400);
  mainWindow.show();
  QApplication::processEvents();

  vtkPVOptions* options = vtkPVOptions::New();
  vtkInitializationHelper::Initialize(argc, argv, options);
  options->Delete();

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
#ifdef REMOTE_CONNECTION_CS
  vtkIdType connectionID = pm->ConnectToRemote("localhost", 11111);
#else
# ifdef REMOTE_CONNECTION_CRS
  vtkIdType connectionID = pm->ConnectToRemote("localhost", 11111,
    "localhost", 22221);
# else
  vtkIdType connectionID = pm->ConnectToSelf();
# endif
#endif

  vtkSMProxy* viewProxy = vtkSMProxyManager::GetProxyManager()->NewProxy("new_views",
    "RenderView2");
  viewProxy->SetConnectionID(connectionID);
  vtkSMPropertyHelper(viewProxy,"Size").Set(0, 400);
  vtkSMPropertyHelper(viewProxy,"Size").Set(1, 400);
  viewProxy->UpdateVTKObjects();
  viewProxy->UpdateVTKObjects();

  vtkPVRenderView* rv = vtkPVRenderView::SafeDownCast(viewProxy->GetClientSideObject());
  QVTKWidget* qwidget = new QVTKWidget(&mainWindow);
  qwidget->SetRenderWindow(rv->GetRenderWindow());
  vtkSMProxy* sphere = addSphere(viewProxy);
  createScalarBar(viewProxy);

  QWidget *centralWidget = new QWidget(&mainWindow);
  QVBoxLayout* vbox = new QVBoxLayout(centralWidget);
  QHBoxLayout* hbox = new QHBoxLayout();
  vbox->addLayout(hbox);
  mainWindow.setCentralWidget(centralWidget);
  hbox->addWidget(qwidget);

  viewProxy->InvokeCommand("StillRender");
  viewProxy->InvokeCommand("ResetCamera");

  pqIntRangeWidget* slider = new pqIntRangeWidget(&mainWindow);
  slider->setMinimum(3);
  slider->setMaximum(1024);
  slider->setStrictRange(true);

  pqPropertyLinks links;
  links.addPropertyLink(slider, "value", SIGNAL(valueChanged(int)),
    sphere, sphere->GetProperty("PhiResolution"));
  links.addPropertyLink(slider, "value", SIGNAL(valueChanged(int)),
    sphere, sphere->GetProperty("ThetaResolution"));
  links.setAutoUpdateVTKObjects(true);
  links.setUseUncheckedProperties(false);
  vbox->addWidget(slider);

  pqIntRangeWidget* slider2 = new pqIntRangeWidget(&mainWindow);
  slider2->setMinimum(0);
  slider2->setMaximum(1000);
  slider2->setStrictRange(true);

 links.addPropertyLink(slider2, "value", SIGNAL(valueChanged(int)),
    viewProxy, viewProxy->GetProperty("RemoteRenderThreshold"));
 links.addPropertyLink(slider2, "value", SIGNAL(valueChanged(int)),
    viewProxy, viewProxy->GetProperty("LODThreshold"));
  vbox->addWidget(slider2);

#ifdef SECOND_WINDOW

  vtkSMProxy* view2Proxy = vtkSMProxyManager::GetProxyManager()->NewProxy("new_views",
    "RenderView2");
  view2Proxy->SetConnectionID(connectionID);
  vtkSMPropertyHelper(view2Proxy, "Size").Set(0, 400);
  vtkSMPropertyHelper(view2Proxy, "Size").Set(1, 400);
  vtkSMPropertyHelper(view2Proxy, "Position").Set(0, 400);
  view2Proxy->UpdateVTKObjects();

  vtkPVRenderView* rv2 = vtkPVRenderView::SafeDownCast(view2Proxy->GetClientSideObject());
  qwidget = new QVTKWidget(&mainWindow);
  qwidget->SetRenderWindow(rv2->GetRenderWindow());

  addSphere(view2Proxy, sphere);
  createScalarBar(view2Proxy);

  hbox->addWidget(qwidget);
  view2Proxy->InvokeCommand("StillRender");
  view2Proxy->InvokeCommand("ResetCamera");

 links.addPropertyLink(slider2, "value", SIGNAL(valueChanged(int)),
    view2Proxy, view2Proxy->GetProperty("RemoteRenderThreshold"));
 links.addPropertyLink(slider2, "value", SIGNAL(valueChanged(int)),
    view2Proxy, view2Proxy->GetProperty("LODThreshold"));
  vbox->addWidget(slider2);
#endif
  links.reset();

  int ret = app.exec();
#ifdef SECOND_WINDOW
  view2Proxy->Delete();
#endif
  viewProxy->Delete();

  vtkInitializationHelper::Finalize();
  return ret;
}

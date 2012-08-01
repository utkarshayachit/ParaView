/*=========================================================================

  Program:   Visualization Toolkit
  Module:    pqThreeSliceView.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME pqThreeSliceView - QT GUI that allow 3 slice control across 4 view embedded as 1

#ifndef __pqThreeSliceView_h
#define __pqThreeSliceView_h

#include "pqRenderView.h"

#include <QtCore>
#include <QtGui>

class QVTKWidget;
class pqRenderView;
class vtkSMPropertyLink;
class vtkSMViewProxy;
class vtkEventQtSlotConnect;

class pqThreeSliceView : public pqRenderView
{
  Q_OBJECT
  typedef pqRenderView Superclass;
public:
  static QString pqThreeSliceViewType() { return "ThreeSliceView"; }
  static QString pqThreeSliceViewTypeName() { return "Three Slice View"; }

  /// constructor takes a bunch of init stuff and must have this signature to
  /// satisfy pqView
  pqThreeSliceView( const QString& viewtype,
                    const QString& group,
                    const QString& name,
                    vtkSMViewProxy* viewmodule,
                    pqServer* server,
                    QObject* p);
  virtual ~pqThreeSliceView();

  /// Override to decorate the QVTKWidget
  virtual QWidget* createWidget();

  /// Redirect reset camera to all views
  virtual void resetCamera();

public slots:
  /// Request a StillRender on idle. Multiple calls are collapsed into one.
  virtual void render();

private:
  pqThreeSliceView(const pqThreeSliceView&); // Not implemented.
  void operator=(const pqThreeSliceView&); // Not implemented.

  class pqInternal;
  pqInternal* Internal;
};

#endif

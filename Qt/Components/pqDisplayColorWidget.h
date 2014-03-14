/*=========================================================================

   Program: ParaView
   Module:    pqDisplayColorWidget.h

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
#ifndef _pqDisplayColorWidget_h
#define _pqDisplayColorWidget_h

#include "pqComponentsModule.h"

#include <QWidget>
#include <QPointer>
#include <QStringList>

class QComboBox;

class pqDataRepresentation;
class vtkEventQtSlotConnect;

/// pqDisplayColorWidget is a widget that can be used to select the array to
/// with for representations (also known as displays). It comprises of two
/// combo-boxes, one for the array name, and another for component number.
/// To use, set the representation using setRepresentation(..). If the
/// representation has appropriate properties, this widget will allow users
/// to select the array name.
class PQCOMPONENTS_EXPORT pqDisplayColorWidget : public QWidget
{
  Q_OBJECT

  Q_PROPERTY(QStringList arraySelection
             READ arraySelection
             WRITE setArraySelection
             NOTIFY arraySelectionChanged);
  Q_PROPERTY(int componentNumber
             READ componentNumber
             WRITE setComponentNumber
             NOTIFY componentNumberChanged);

  typedef QWidget Superclass;
public:
  pqDisplayColorWidget(QWidget *parent=0);
  ~pqDisplayColorWidget();

  /// Get/Set the array name. The QStringList has 2 elements,
  /// (association-number, arrayname).
  QStringList arraySelection() const;
  void setArraySelection(const QStringList&);
  QString getCurrentText() const
    {
    QStringList val = this->arraySelection();
    return val.size() == 2? val[1] : QString();
    }

  /// Get/Set the component number.
  int componentNumber() const;
  void setComponentNumber(int);

signals:
  void componentNumberChanged();
  void arraySelectionChanged();

public slots:
  /// Set the representation to control the scalar coloring properties on.
  void setRepresentation(pqDataRepresentation* display);

private slots:
  void refreshColorArrayNames();

private:
  QVariant itemData(int association, const QString& arrayName) const;
  QIcon* itemIcon(int association, const QString& arrayName) const;

private:
  QIcon* CellDataIcon;
  QIcon* PointDataIcon;
  QIcon* SolidColorIcon;
  QComboBox* Variables;
  QComboBox* Components;
  QPointer<pqDataRepresentation> Representation;

  // This is maintained to detect when the representation has changed.
  void* CachedRepresentation;

  class pqInternals;
  pqInternals* Internals;
};
#endif

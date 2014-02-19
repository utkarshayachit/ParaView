/*=========================================================================

   Program: ParaView
   Module:    pqLookupTableManager.h

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
#ifndef __pqLookupTableManager_h
#define __pqLookupTableManager_h

#include <QObject>
#include "pqCoreModule.h"

class pqDataRepresentation;
class pqProxy;
class pqScalarBarRepresentation;
class pqScalarsToColors;
class pqScalarOpacityFunction;
class pqServer;
class vtkSMProxy;

/// pqLookupTableManager is the manager that manages color lookup objects.
/// It uses vtkSMTransferFunctionManager instance to manage color transfer
/// functions and opacity transfer functions.
class PQCORE_EXPORT pqLookupTableManager : public QObject
{
  Q_OBJECT
public:
  pqLookupTableManager(QObject* parent=NULL);
  virtual ~pqLookupTableManager();

  /// Get a LookupTable for the array with name \c arrayname 
  /// and component. component = -1 represents magnitude.
  /// Simply calls vtkSMTransferFunctionManager::GetColorTransferFunction(...).
  /// The component number is no longer used.
  virtual pqScalarsToColors* getLookupTable(pqServer* server, 
    const QString& arrayname, int number_of_components, int component=0);
    
  /// Returns the pqScalarOpacityFunction object for the piecewise
  /// function used to map scalars to opacity.
  virtual pqScalarOpacityFunction* getScalarOpacityFunction(pqServer* server, 
    const QString& arrayname, int number_of_components, int component=0);

  /// Set the scalar bar's visibility for the given lookup table in the given
  /// view. This may result in creating of a new scalar bar.
  virtual void setScalarBarVisibility(pqDataRepresentation* repr,  bool visible);

  /// Saves the state of the lut/opacity-function so that 
  /// the next time a new LUT/opacity-function is created, it
  /// will have the same state as this one.
  virtual void saveLUTAsDefault(pqScalarsToColors*){};
  virtual void saveOpacityFunctionAsDefault(pqScalarOpacityFunction*){}

  /// save the state of the scalar bar, so that the next time a new scalar bar
  /// is created its properties are setup using the defaults specified.
  virtual void saveScalarBarAsDefault(pqScalarBarRepresentation*){}
  
  /// Given a "PVLookupTable" proxy, saves the color/opacity and scalar bar
  /// properties as default.
  void saveAsDefault(vtkSMProxy* lutProxy, vtkSMProxy* viewProxy=NULL){}
};
#endif


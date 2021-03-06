/*=========================================================================

   Program: ParaView
   Module:    pqRubberBandHelper.h

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

#ifndef __pqRubberBandHelper_h
#define __pqRubberBandHelper_h

#include "pqCoreExport.h"
#include <QObject>

class pqRenderView;
class pqView;

/*! \brief Utility to switch interactor styles in 3D views.
 *
 * pqRubberBandHelper standardizes the mechanism by which 3D views
 * are switched between interaction and rubber band modes.
 * TODO: We may want to extend this class to create different type of selections
 * i.e. surface/frustrum.
 */
class PQCORE_EXPORT pqRubberBandHelper : public QObject
{
  Q_OBJECT
public:
  pqRubberBandHelper(QObject* parent=NULL);
  virtual ~pqRubberBandHelper();

  static void ReorderBoundingBox(int src[4], int dest[4]);

  //for internal use only, this is how mouse press and release events
  //are processed internally
  void processEvents(unsigned long event);

  /// Returns the currently selected render view.
  pqRenderView* getRenderView() const;

  //BTX
  enum Modes
  {
    INTERACT,
    SELECT, //aka, Surface selection
    SELECT_POINTS,
    FRUSTUM,
    FRUSTUM_POINTS,
    BLOCKS,
    ZOOM
  };
  //ETX

public slots:
  /// Set active view. If a view has been set previously, this method ensures
  /// that it is not in selection mode.
  void setView(pqView*);

  /// Begin rubber band surface selection on the view. 
  /// Has any effect only if active view is a render view.
  void beginSurfaceSelection();
  void beginSurfacePointsSelection();
  void beginFrustumSelection();
  void beginFrustumPointsSelection();
  void beginBlockSelection();
  void beginZoom();

  /// End rubber band selection.
  /// Has any effect only if active view is a render view.
  void endSelection();
  void endZoom()
    { this->endSelection(); }

  /// Called to disable selection.
  void DisabledPush();
  
  /// Called to pop disabling of selection. If there are as many DisabledPop() as
  /// DisabledPush(), the selection will be enabled.
  void DisabledPop();

signals:
  /// fired after mouse up in selection mode
  void selectionFinished(int xmin, int ymin, int xmax, int ymax);

  /// Fired to indicate whether the selection can be create on the currently set
  /// view.
  void enableSurfaceSelection(bool enabled);
  void enableSurfacePointsSelection(bool enabled);
  void enableFrustumSelection(bool enabled);
  void enableFrustumPointSelection(bool enabled);
  void enableBlockSelection(bool enabled);
  void enableZoom(bool enabled);

  /// Fired with selection mode changes. 
  /// \c selectionMode is enum Modes{...}.
  void selectionModeChanged(int selectionMode);

  /// This is inverse of selectionModeChanged signal, provided for convenience.
  void interactionModeChanged(bool notselectable);

  /// Fired to mark the start and ends of election.
  void startSelection();
  void stopSelection();

protected slots:
  void emitEnabledSignals();

protected:
  int setRubberBandOn(int mode);
  int setRubberBandOff();
  int Mode;
  int Xs, Ys, Xe, Ye;
  int DisableCount;

private:
  class pqInternal;
  class vtkPQSelectionObserver;

  pqInternal* Internal;
};

#endif


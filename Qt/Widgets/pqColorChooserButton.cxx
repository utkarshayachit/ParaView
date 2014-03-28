/*=========================================================================

   Program: ParaView
   Module:  pqColorChooserButton.cxx

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

// self includes
#include "pqColorChooserButton.h"

// Qt includes
#include <QColorDialog>
#include <QPainter>
#include <QResizeEvent>

//-----------------------------------------------------------------------------
pqColorChooserButton::pqColorChooserButton(QWidget* p)
  : QToolButton(p)
{
  this->IconRadiusHeightRatio = 0.75;
  this->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  this->connect(this, SIGNAL(clicked()), SLOT(chooseColor()));
}

//-----------------------------------------------------------------------------
QColor pqColorChooserButton::chosenColor() const
{
  return this->Color;
}

//-----------------------------------------------------------------------------
QVariantList pqColorChooserButton::chosenColorRgbF() const
{
  QVariantList val;
  val << this->Color.redF() << this->Color.greenF() << this->Color.blueF();
  return val;
}

//-----------------------------------------------------------------------------
QVariantList pqColorChooserButton::chosenColorRgbaF() const
{
  QVariantList val;
  val << this->Color.redF() << this->Color.greenF()
    << this->Color.blueF() << this->Color.alphaF();
  return val;
}

//-----------------------------------------------------------------------------
void pqColorChooserButton::setChosenColor(const QColor& color)
{
  if (color.isValid())
    {
    if(color != this->Color)
      {
      this->Color = color;
      this->setIcon(this->renderColorSwatch(color));
      emit this->chosenColorChanged(this->Color);
      }
    emit this->validColorChosen(this->Color);
    }
}

//-----------------------------------------------------------------------------
void pqColorChooserButton::setChosenColorRgbF(const QVariantList& val)
{
  Q_ASSERT(val.size() == 3);
  QColor newColor = this->chosenColor();
  newColor.setRedF(val[0].toDouble());
  newColor.setGreenF(val[1].toDouble());
  newColor.setBlueF(val[2].toDouble());
  this->setChosenColor(newColor);
}

//-----------------------------------------------------------------------------
void pqColorChooserButton::setChosenColorRgbaF(const QVariantList& val)
{
  Q_ASSERT(val.size() == 4);
  QColor newColor;
  newColor.setRedF(val[0].toDouble());
  newColor.setGreenF(val[1].toDouble());
  newColor.setBlueF(val[2].toDouble());
  newColor.setAlphaF(val[2].toDouble());
  this->setChosenColor(newColor);
}

//-----------------------------------------------------------------------------
QIcon pqColorChooserButton::renderColorSwatch(const QColor& color)
{
  int radius = qRound(this->height() * this->IconRadiusHeightRatio);
  if (radius <= 10)
    {
    radius = 10;
    }

  QPixmap pix(radius, radius);
  pix.fill(QColor(0,0,0,0));

  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setBrush(QBrush(color));
  painter.drawEllipse(1, 1, radius-2, radius-2);
  painter.end();
  return QIcon(pix);
}

//-----------------------------------------------------------------------------
void pqColorChooserButton::chooseColor()
{
  this->setChosenColor(QColorDialog::getColor(this->Color, this));
}

//-----------------------------------------------------------------------------
void pqColorChooserButton::resizeEvent(QResizeEvent *rEvent)
{
  (void) rEvent;

  this->setIcon(this->renderColorSwatch(this->Color));
}

//************************************************************************
//									*
//		     Copyright (C)  2006				*
//     University Corporation for Atmospheric Research			*
//		     All Rights Reserved				*
//									*
//************************************************************************/
//
//	File:		AnnotationEventRouter.h
//
//	Author:		Alan Norton
//			National Center for Atmospheric Research
//			PO 3000, Boulder, Colorado
//
//	Date:		June, 2015
//
//	Description:	Defines the AnnotationEventRouter class.
//		This class handles events for the Annotation params
//
#ifndef ANNOTATIONEVENTROUTER_H
#define ANNOTATIONEVENTROUTER_H

#include <qobject.h>
#include "EventRouter.h"
#include <vapor/MyBase.h>
#include "ui_AnnotationGUI.h"
#include "RangeCombos.h"
#include "VaporTable.h"
#include <vapor/AxisAnnotation.h>

QT_USE_NAMESPACE

namespace VAPoR {
class ControlExec;
}

class PGroup;
class PSection;
class CopyRegionAnnotationWidget;
class VComboBox;
class VPushButton;

class AnnotationEventRouter : public QWidget, public EventRouter {
    Q_OBJECT

public:
    AnnotationEventRouter(QWidget *parent, VAPoR::ControlExec *ce);

    virtual ~AnnotationEventRouter();

    virtual void hookUpTab(){};

    virtual void GetWebHelp(std::vector<std::pair<string, string>> &help) const;

    //! Ignore wheel event in tab (to avoid confusion)
    virtual void wheelEvent(QWheelEvent *) {}

    // Get static string identifier for this router class
    //
    static string GetClassType() { return ("Annotation"); }
    string        GetType() const { return GetClassType(); }

    virtual void _confirmText(){};

protected slots:
    void axisAnnotationTableChanged();
    void copyRegionFromRenderer();

private:
    Combo *     _textSizeCombo;
    Combo *     _digitsCombo;
    Combo *     _ticWidthCombo;

    std::map<std::string, std::string> _visNames;
    std::map<std::string, std::string> _renTypeNames;

    vector<double> getTableRow(int row);

    AnnotationEventRouter() {}

    void setColorHelper(QWidget *w, vector<double> &rgb);
    void updateColorHelper(const vector<double> &rgb, QWidget *w);

    void updateAxisAnnotations();
    void updateAxisTable();
    void updateCopyRegionCombo();
    void gatherRenderers(std::vector<string> &, string, string, string, string);

    void   updateDataMgrCombo();
    string getProjString();

    VAPoR::AxisAnnotation *_getCurrentAxisAnnotation();

    std::vector<double> getDomainExtents() const;
    void                scaleNormalizedCoordsToWorld(std::vector<double> &coords);
    void                scaleWorldCoordsToNormalized(std::vector<double> &coords);
    void                convertPCSToLon(double &xCoord);
    void                convertPCSToLat(double &yCoord);
    void                convertPCSToLonLat(double &xCoord, double &yCoord);
    void                convertLonLatToPCS(double &xCoord, double &yCoord);
    void                convertLonToPCS(double &xCoord);
    void                convertLatToPCS(double &yCoord);

    virtual void _updateTab();

    AnimationParams *_ap;
    bool             _animConnected;

    VaporTable *                _annotationVaporTable;
    CopyRegionAnnotationWidget *_copyRegionWidget;
    VComboBox *                 _copyRegionCombo;
    VPushButton *               _copyRegionButton;

    PGroup *  _axisAnnotationGroup1;
    PGroup *  _axisAnnotationGroup2;
    PGroup *  _axisArrowGroup;
    PGroup *  _timeAnnotationGroup;
    PGroup *  _3DGeometryGroup;
};

#endif    // ANNOTATIONEVENTROUTER_H

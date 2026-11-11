#include "SimulationCurveDialog.h"
#include "istar_ui_stylesheet.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

using namespace ISTAR::GUI;
using namespace ISTAR::UIStyle;

#include "istar_language.h"
#define TR_STR(msg)  QString::fromStdString(Tr(msg))

SimulationCurveDialog::SimulationCurveDialog(QWidget *parent)
    : QDialog(parent)
    , p_Chart(new CustomChart)
{
    setWindowTitle(TR_STR("Simulation Curve"));
    resize(822, 492);

    {
        m_AxisX.name = TR_STR("Time");
        m_AxisX.unit = QStringLiteral("s");
        m_AxisX.mode = 1;
        m_AxisX.color = QColor(0, 0, 0);
    }
    {
        m_AxisYTUp.name = TR_STR("Up Chuck Temperature");
        m_AxisYTUp.unit = QStringLiteral("℃");
        m_AxisYTUp.mode = 0;
        m_AxisYTUp.range.start = 0;
        m_AxisYTUp.range.end = 450;
        m_AxisYTUp.color = QColor(200, 0, 0);
    }
    {
        m_AxisYTDw.name = TR_STR("Down Chuck Temperature");
        m_AxisYTDw.unit = QStringLiteral("℃");
        m_AxisYTDw.mode = 0;
        m_AxisYTDw.range.start = 0;
        m_AxisYTDw.range.end = 450;
        m_AxisYTDw.color = QColor(0, 200, 0);
    }
    {
        m_AxisYF.name = TR_STR("Bond Force");
        m_AxisYF.unit = QStringLiteral("N");
        m_AxisYF.mode = 1;
        m_AxisYF.color = QColor(0, 0, 200);
    }

    p_Chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectAxes);
    p_Chart->SetAxisParameter(m_AxisX, { m_AxisYTUp, m_AxisYTDw, m_AxisYF });

    auto *vLayout = new QVBoxLayout(this);
    vLayout->addWidget(p_Chart);

    auto *btnClose = new QPushButton(TR_STR("Close"));
    btnClose->setStyleSheet(iStarUIStyleSheet->Button_Border);
    btnClose->setFixedSize(60,40);
    auto *hLayout = new QHBoxLayout;
    hLayout->addStretch();
    hLayout->addWidget(btnClose);
    hLayout->addStretch();
    vLayout->addLayout(hLayout);

    QObject::connect(btnClose, &QPushButton::clicked, this, &SimulationCurveDialog::on_btnClose_clicked);
}

void SimulationCurveDialog::SetTheoryBondData(const TheoryBondData &data)
{
    p_Chart->ClearView();

    QHash<QString, QPointF> dic_dVal;
    for (const QPointF &e : data.vec_TempUp)
    {
        dic_dVal[m_AxisYTUp.name] = e;
        p_Chart->AddAxisData(dic_dVal);
    }
    dic_dVal.clear();
    for (const QPointF &e : data.vec_TempDw)
    {
        dic_dVal[m_AxisYTDw.name] = e;
        p_Chart->AddAxisData(dic_dVal);
    }
    dic_dVal.clear();
    for (const QPointF &e : data.vec_Force)
    {
        dic_dVal[m_AxisYF.name] = e;
        p_Chart->AddAxisData(dic_dVal);
    }
}

void SimulationCurveDialog::on_btnClose_clicked()
{
    this->accept();
}

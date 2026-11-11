#ifndef SIMULATIONCURVEDIALOG_H
#define SIMULATIONCURVEDIALOG_H

#include <QDialog>
#include "chart/customchart.h"
#include "TheoryBondData.h"

// 理论键合曲线对话框（模仿平台 SimulationCurveDialog：时间-温度/力 多轴曲线）
class SimulationCurveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SimulationCurveDialog(QWidget *parent = nullptr);

    void SetTheoryBondData(const TheoryBondData &data);

private slots:
    void on_btnClose_clicked();

private:
    ISTAR::GUI::CustomChart *p_Chart = nullptr;
    ISTAR::GUI::CustomChart::AxisParameter m_AxisX;
    ISTAR::GUI::CustomChart::AxisParameter m_AxisYTUp;
    ISTAR::GUI::CustomChart::AxisParameter m_AxisYTDw;
    ISTAR::GUI::CustomChart::AxisParameter m_AxisYF;
};

#endif // SIMULATIONCURVEDIALOG_H

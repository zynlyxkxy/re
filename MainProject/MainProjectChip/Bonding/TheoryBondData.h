#ifndef THEORYBONDDATA_H
#define THEORYBONDDATA_H

#include <QVector>
#include <QPointF>

// 理论键合曲线数据（仅用于模拟曲线展示，不参与配方序列化）
struct TheoryBondData
{
    QVector<QPointF> vec_TempUp;   // 上吸盘温度 x=时间(s) y=温度(℃)
    QVector<QPointF> vec_TempDw;   // 下吸盘温度 x=时间(s) y=温度(℃)
    QVector<QPointF> vec_Force;    // 键合力     x=时间(s) y=力(N)
};

#endif // THEORYBONDDATA_H

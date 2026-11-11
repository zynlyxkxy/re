#ifndef EGA_FIT_H
#define EGA_FIT_H

#include <cmath>
#include <algorithm>
#include <vector>

namespace EGA {

// ============ 简单二维向量 ============
struct Vector2d {
    double x, y;
    Vector2d() : x(0.0), y(0.0) {}
    Vector2d(double xx, double yy) : x(xx), y(yy) {}
    double& operator()(int i) { return i == 0 ? x : y; }
    double operator()(int i) const { return i == 0 ? x : y; }
    double norm() const { return std::sqrt(x * x + y * y); }
    Vector2d operator+(const Vector2d& o) const { return Vector2d(x + o.x, y + o.y); }
    Vector2d operator-(const Vector2d& o) const { return Vector2d(x - o.x, y - o.y); }
};

// ============ 简单动态矩阵（行优先存储），替代 Eigen 矩阵类型 ============
class DenseMatrix;
class RowProxy;
class ConstRowProxy;
class ColProxy;
class ConstColProxy;

class DenseMatrix {
public:
    DenseMatrix() : rows_(0), cols_(0) {}
    DenseMatrix(int n) : rows_(n), cols_(1), data_(n, 0.0) {}   // 列向量
    DenseMatrix(int rows, int cols) : rows_(rows), cols_(cols), data_(rows * cols, 0.0) {}

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    int size() const { return rows_ * cols_; }

    // 双下标访问 (i,j)
    double& operator()(int i, int j) { return data_[i * cols_ + j]; }
    double operator()(int i, int j) const { return data_[i * cols_ + j]; }

    // 线性访问（行优先）
    double& operator()(int i) { return data_[i]; }
    double operator()(int i) const { return data_[i]; }
    double& operator[](int i) { return data_[i]; }
    double operator[](int i) const { return data_[i]; }

    RowProxy row(int i);
    ConstRowProxy row(int i) const;
    ColProxy col(int j);
    ConstColProxy col(int j) const;

    // 最小二乘解：正规方程 A^T A x = A^T b + 高斯消元（部分选主元）
    DenseMatrix solve(const DenseMatrix& b) const;

private:
    int rows_, cols_;
    std::vector<double> data_;
};

// ============ 行 / 列代理 ============
class ConstRowProxy {
    const DenseMatrix* m_;
    int r_;
public:
    ConstRowProxy(const DenseMatrix* m, int r) : m_(m), r_(r) {}
    double operator()(int j) const { return (*m_)(r_, j); }
    double operator[](int j) const { return (*m_)(r_, j); }
    double norm() const {
        double s = 0.0;
        for (int j = 0; j < m_->cols(); ++j) {
            double v = (*m_)(r_, j);
            s += v * v;
        }
        return std::sqrt(s);
    }
    operator Vector2d() const { return Vector2d((*m_)(r_, 0), (*m_)(r_, 1)); }
};

class RowProxy {
    DenseMatrix* m_;
    int r_;
public:
    RowProxy(DenseMatrix* m, int r) : m_(m), r_(r) {}
    double& operator()(int j) { return (*m_)(r_, j); }
    double operator()(int j) const { return (*m_)(r_, j); }
    double& operator[](int j) { return (*m_)(r_, j); }
    double operator[](int j) const { return (*m_)(r_, j); }
    double norm() const {
        double s = 0.0;
        for (int j = 0; j < m_->cols(); ++j) {
            double v = (*m_)(r_, j);
            s += v * v;
        }
        return std::sqrt(s);
    }
    RowProxy& operator=(const Vector2d& v) {
        (*m_)(r_, 0) = v.x;
        (*m_)(r_, 1) = v.y;
        return *this;
    }
    operator Vector2d() const { return Vector2d((*m_)(r_, 0), (*m_)(r_, 1)); }
};

class ConstColProxy {
    const DenseMatrix* m_;
    int c_;
public:
    ConstColProxy(const DenseMatrix* m, int c) : m_(m), c_(c) {}
    double operator()(int i) const { return (*m_)(i, c_); }
    double operator[](int i) const { return (*m_)(i, c_); }
    double norm() const {
        double s = 0.0;
        for (int i = 0; i < m_->rows(); ++i) {
            double v = (*m_)(i, c_);
            s += v * v;
        }
        return std::sqrt(s);
    }
    operator Vector2d() const { return Vector2d((*m_)(0, c_), (*m_)(1, c_)); }
    Vector2d operator-(const Vector2d& o) const { return Vector2d((*m_)(0, c_) - o.x, (*m_)(1, c_) - o.y); }
};

class ColProxy {
    DenseMatrix* m_;
    int c_;
public:
    ColProxy(DenseMatrix* m, int c) : m_(m), c_(c) {}
    double& operator()(int i) { return (*m_)(i, c_); }
    double operator()(int i) const { return (*m_)(i, c_); }
    double& operator[](int i) { return (*m_)(i, c_); }
    double operator[](int i) const { return (*m_)(i, c_); }
    double norm() const {
        double s = 0.0;
        for (int i = 0; i < m_->rows(); ++i) {
            double v = (*m_)(i, c_);
            s += v * v;
        }
        return std::sqrt(s);
    }
    ColProxy& operator=(const Vector2d& v) {
        (*m_)(0, c_) = v.x;
        (*m_)(1, c_) = v.y;
        return *this;
    }
    ColProxy& operator=(const ConstColProxy& o) {
        for (int i = 0; i < m_->rows(); ++i) (*m_)(i, c_) = o(i);
        return *this;
    }
    operator Vector2d() const { return Vector2d((*m_)(0, c_), (*m_)(1, c_)); }
    Vector2d operator-(const Vector2d& o) const { return Vector2d((*m_)(0, c_) - o.x, (*m_)(1, c_) - o.y); }
};

inline RowProxy DenseMatrix::row(int i) { return RowProxy(this, i); }
inline ConstRowProxy DenseMatrix::row(int i) const { return ConstRowProxy(this, i); }
inline ColProxy DenseMatrix::col(int j) { return ColProxy(this, j); }
inline ConstColProxy DenseMatrix::col(int j) const { return ConstColProxy(this, j); }

inline DenseMatrix DenseMatrix::solve(const DenseMatrix& b) const {
    int n = cols_;
    DenseMatrix ata(n, n);
    DenseMatrix atb(n, 1);

    // 构造正规方程
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double s = 0.0;
            for (int k = 0; k < rows_; ++k)
                s += data_[k * cols_ + i] * data_[k * cols_ + j];
            ata(i, j) = s;
        }
        double sb = 0.0;
        for (int k = 0; k < rows_; ++k)
            sb += data_[k * cols_ + i] * b(k, 0);
        atb(i, 0) = sb;
    }

    // 高斯消元（部分选主元）
    for (int k = 0; k < n; ++k) {
        int piv = k;
        double mx = std::fabs(ata(k, k));
        for (int r = k + 1; r < n; ++r) {
            double v = std::fabs(ata(r, k));
            if (v > mx) { mx = v; piv = r; }
        }
        if (piv != k) {
            for (int c = 0; c < n; ++c) std::swap(ata(k, c), ata(piv, c));
            std::swap(atb(k, 0), atb(piv, 0));
        }
        for (int r = k + 1; r < n; ++r) {
            double factor = ata(r, k) / ata(k, k);
            for (int c = k; c < n; ++c) ata(r, c) -= factor * ata(k, c);
            atb(r, 0) -= factor * atb(k, 0);
        }
    }

    // 回代
    DenseMatrix x(n, 1);
    for (int i = n - 1; i >= 0; --i) {
        double s = atb(i, 0);
        for (int j = i + 1; j < n; ++j) s -= ata(i, j) * x(j, 0);
        x(i, 0) = s / ata(i, i);
    }
    return x;
}

// ============ 类型别名，保持原有接口不变 ============
using MatrixXd = DenseMatrix;
using VectorXd = DenseMatrix;
using Matrix2Xd = DenseMatrix;

// 晶圆芯片位置生成；wafer_notch_orient 按切边角度变换坐标：
// 0→(x,y) 90→(y,x) 180→(-x,-y) 270→(-y,-x)
MatrixXd create_chip_position(double wafer_radius,
                              double chip_width,
                              double chip_height,
                              double spacing_x, double spacing_y,
                              bool align_to_corner = false,
                              int wafer_notch_orient = 0);

// 计算标记点位置
Matrix2Xd compute_mark_positions(const Matrix2Xd& centers,
                                 const std::vector<Vector2d> &markOffsets);

// 模拟实际位置 (测试用)
Matrix2Xd simulate_real_positions(const Matrix2Xd& nominal_positions,
                                  const VectorXd& params,
                                  double noise_sigma = 0.0);

// 选择拟合点
Matrix2Xd select_fitting_points(const Matrix2Xd& nominal_positions,
                                int points_number,
                                double wafer_radius,
                                double select_range);

// EGA 拟合
VectorXd ega_fit(const Matrix2Xd& fit_points,
                 const Matrix2Xd& real_points);

// 坐标变换
Matrix2Xd transform_layout_to_coarse_coords(const Matrix2Xd& nominal_points,
                                            const VectorXd& params);

} // namespace EGA

#endif // EGA_FIT_H

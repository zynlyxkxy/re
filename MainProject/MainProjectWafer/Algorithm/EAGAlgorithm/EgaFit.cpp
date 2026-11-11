#include "EgaFit.h"
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace EGA {

MatrixXd create_chip_position(double wafer_radius,
                              double chip_width,
                              double chip_height,
                              double spacing_x, double spacing_y,
                              bool align_to_corner,
                              int wafer_notch_orient) {
    double step_x = chip_width + spacing_x;
    double step_y = chip_height + spacing_y;

    // 生成坐标序列的辅助函数
    auto get_coords = [](double step, double radius, bool to_corner) -> std::vector<double>
    {
        // 计算覆盖范围
        double half_range = std::ceil(radius / step) * step;

        // 生成等差数列
        std::vector<double> coords;
        for(double i = -half_range; i < half_range + step; i += step)
        {
            coords.push_back(i);
        }

        if(to_corner)
        {
            // 对齐到交汇点，偏移半个步长
            double current_min_pos = half_range;
            for (double x : coords) {
                if (x >= 0 && x < current_min_pos) current_min_pos = x;
            }
            for (double& x : coords) x = x - current_min_pos + (step / 2);
        }else{
            // 对齐到芯片中心，确保0是芯片中心
            double current_min_pos = half_range;
            for (double x : coords) {
                if (x >= 0 && x < current_min_pos) current_min_pos = x;
            }
            for (double& x : coords) x -= current_min_pos;
        }
        return coords;
    };

    // 生成X和Y坐标序列
    std::vector<double> x_coords = get_coords(step_x, wafer_radius, align_to_corner);
    std::vector<double> y_coords = get_coords(step_y, wafer_radius, align_to_corner);

//    // 生成x坐标
//    std::vector<double> x_coords;
//    for (double x = -wafer_radius + step_x/2; x < wafer_radius; x += step_x) {
//        x_coords.push_back(x);
//    }

//    // 调整x坐标原点
//    double min_positive_x = wafer_radius;
//    for (double x : x_coords) {
//        if (x >= 0 && x < min_positive_x) min_positive_x = x;
//    }
//    for (double& x : x_coords) x -= min_positive_x;

//    // 生成y坐标
//    std::vector<double> y_coords;
//    for (double y = -wafer_radius + step_y/2; y < wafer_radius; y += step_y) {
//        y_coords.push_back(y);
//    }

//    // 调整y坐标原点
//    double min_positive_y = wafer_radius;
//    for (double y : y_coords) {
//        if (y >= 0 && y < min_positive_y) min_positive_y = y;
//    }
//    for (double& y : y_coords) y -= min_positive_y;

    // 构建网格点
    MatrixXd grid_positions(x_coords.size() * y_coords.size(), 2);
    int idx = 0;
    for (double y : y_coords) {
        for (double x : x_coords) {
            grid_positions(idx, 0) = x;
            grid_positions(idx, 1) = y;
            ++idx;
        }
    }

    // 过滤超出边界的点
    double corner_dist = std::sqrt(pow(chip_width/2, 2) + pow(chip_height/2, 2));
    std::vector<Vector2d> valid_points;
    for (int i = 0; i < grid_positions.rows(); ++i) {
        double dist = grid_positions.row(i).norm();
        if (dist + corner_dist < wafer_radius) {
            valid_points.push_back(grid_positions.row(i));
        }
    }

    // 转换为Eigen矩阵
    MatrixXd result(valid_points.size(), 2);
    for (int i = 0; i < valid_points.size(); ++i) {
        result.row(i) = valid_points[i];
    }

    // 按切边角度变换坐标：0°→(x,y)，90°→(y,x)，180°→(-x,-y)，270°→(-y,-x)
    for (int i = 0; i < result.rows(); ++i)
    {
        const double px = result(i, 0);
        const double py = result(i, 1);
        switch (wafer_notch_orient)
        {
            case 90:
                result(i, 0) = py;
                result(i, 1) = px;
                break;
            case 180:
                result(i, 0) = -px;
                result(i, 1) = -py;
                break;
            case 270:
                result(i, 0) = -py;
                result(i, 1) = -px;
                break;
            default:   // 0 及其他角度保持不变
                break;
        }
    }
    return result;
}

Matrix2Xd compute_mark_positions(const Matrix2Xd& centers, const std::vector<Vector2d> &markOffsets) {
    std::vector<Vector2d> marks;
    for (int i = 0; i < centers.cols(); ++i) {
        Vector2d center = centers.col(i);
        for(auto &i : markOffsets)
        {
            marks.push_back(center + i);
        }
    }

    // 转换为Eigen矩阵
    Matrix2Xd result(2, marks.size());
    for (int i = 0; i < marks.size(); ++i) {
        result.col(i) = marks[i];
    }
    return result;
}

Matrix2Xd simulate_real_positions(const Matrix2Xd& nominal_positions,
                                  const VectorXd& params,
                                  double noise_sigma) {
    Matrix2Xd real_positions(2, nominal_positions.cols());
    for (int i = 0; i < nominal_positions.cols(); ++i) {
        double u = nominal_positions(0, i);
        double v = nominal_positions(1, i);
        double x = params[0] + params[2]*u - params[4]*v;
        double y = params[1] + params[3]*v + params[5]*u;
        real_positions.col(i) = Vector2d(x, y);
    }

    // 添加噪声
    if (noise_sigma > 0) {
        std::default_random_engine generator;
        std::normal_distribution<double> dist(0.0, noise_sigma);
        for (int i = 0; i < real_positions.size(); ++i) {
            real_positions(i) += dist(generator);
        }
    }
    return real_positions;
}

Matrix2Xd select_fitting_points(const Matrix2Xd& nominal_positions,
                                int points_number,
                                double wafer_radius, double select_range) {
    double target_radius = wafer_radius * select_range;
    Matrix2Xd fit_points(2, points_number);

    double M_PI = 3.14159265358979323846;
    for (int i = 0; i < points_number; ++i) {
        double angle = 2 * M_PI * i / points_number;
        Vector2d target(target_radius * std::cos(angle),
                        target_radius * std::sin(angle));

        int closest_idx = 0;
        double min_dist = std::numeric_limits<double>::max();

        for (int j = 0; j < nominal_positions.cols(); ++j) {
            double dist = (nominal_positions.col(j) - target).norm();
            if (dist < min_dist) {
                min_dist = dist;
                closest_idx = j;
            }
        }
        fit_points.col(i) = nominal_positions.col(closest_idx);
    }
    return fit_points;
}

VectorXd ega_fit(const Matrix2Xd& fit_points,
                 const Matrix2Xd& real_points) {
    int n = fit_points.cols();
    MatrixXd A(2*n, 6);
    VectorXd b(2*n);

    // 构建线性系统 AX = b
    for (int i = 0; i < n; ++i) {
        double u = fit_points(0, i);
        double v = fit_points(1, i);
        double x = real_points(0, i);
        double y = real_points(1, i);

        // x方程
        A(2*i, 0) = 1; A(2*i, 1) = 0;
        A(2*i, 2) = u; A(2*i, 3) = 0;
        A(2*i, 4) = -v; A(2*i, 5) = 0;

        // y方程
        A(2*i+1, 0) = 0; A(2*i+1, 1) = 1;
        A(2*i+1, 2) = 0; A(2*i+1, 3) = v;
        A(2*i+1, 4) = 0; A(2*i+1, 5) = u;

        b(2*i) = x;
        b(2*i+1) = y;
    }

    // 求解线性最小二乘问题
    VectorXd params = A.solve(b);
    return params;
}

Matrix2Xd transform_layout_to_coarse_coords(const Matrix2Xd& nominal_points,
                                            const VectorXd& params) {
    Matrix2Xd coarse_coords(2, nominal_points.cols());
    for (int i = 0; i < nominal_points.cols(); ++i) {
        double u = nominal_points(0, i);
        double v = nominal_points(1, i);
        double x = params[0] + params[2]*u - params[4]*v;
        double y = params[1] + params[3]*v + params[5]*u;
        coarse_coords.col(i) = Vector2d(x, y);
    }
    return coarse_coords;
}

} // namespace EGA

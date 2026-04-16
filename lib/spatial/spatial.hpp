#pragma once

#include <cmath>

struct Quaternion {
    float w, x, y, z;
};

struct Vector3 {
    float x, y, z;
};

struct Euler {
    float roll, pitch, yaw;
};

struct Matrix3x3 {
    float m[3][3];
};

namespace spatial {

void quaternion_to_euler(const Quaternion& q, Euler& e);
void quaternion_to_euler_deg(const Quaternion& q, Euler& e_deg);
void vector_multiply_plus(const Vector3& v1, const Vector3& v2, float coef, Vector3& out);
float vector_norm(const Vector3& v);
void quaternion_to_rotation_matrix(const Quaternion& q, Matrix3x3& R);
void rotation_matrix_to_quaternion(const Matrix3x3& R, Quaternion& q);
void quaternion_multiply(const Quaternion& q1, const Quaternion& q2, Quaternion& out);
void quaternion_to_axis_angle(const Quaternion& q, Vector3& axis, float& angle);
void rotation_diff_quaternions(const Quaternion& q1, const Quaternion& q2, Vector3& axis, float& angle);

} // namespace spatial

// C-compatible wrappers for transitional use
extern "C" {
void spatial_quaternion_to_euler(const Quaternion* q, Euler* e);
void spatial_quaternion_to_euler_deg(const Quaternion* q, Euler* e_deg);
void spatial_vector_multiply_plus(const Vector3* v1, const Vector3* v2, float coef, Vector3* v);
float spatial_vector_norm(const Vector3* v);
void spatial_quaternion_to_rotation_matrix(const Quaternion* q, Matrix3x3* R);
void spatial_rotation_matrix_to_quaternion(const Matrix3x3* R, Quaternion* q);
void spatial_quaternion_multiply(const Quaternion* q1, const Quaternion* q2, Quaternion* q);
void spatial_quaternion_to_axis_angle(const Quaternion* q, Vector3* axis, float* angle);
void spatial_rotation_diff_quaternions(const Quaternion* q1, const Quaternion* q2, Vector3* axis, float* angle);
}

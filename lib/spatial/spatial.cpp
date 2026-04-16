#include "spatial.hpp"

namespace spatial {

void quaternion_to_euler(const Quaternion& q, Euler& e) {
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    e.roll = atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (fabsf(sinp) >= 1.0f) {
        e.pitch = copysignf(static_cast<float>(M_PI) / 2.0f, sinp);
    } else {
        e.pitch = asinf(sinp);
    }

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    e.yaw = atan2f(siny_cosp, cosy_cosp);
}

void quaternion_to_euler_deg(const Quaternion& q, Euler& e_deg) {
    Euler e_rad;
    quaternion_to_euler(q, e_rad);
    constexpr float RAD_TO_DEG = 180.0f / static_cast<float>(M_PI);
    e_deg.roll = e_rad.roll * RAD_TO_DEG;
    e_deg.pitch = e_rad.pitch * RAD_TO_DEG;
    e_deg.yaw = e_rad.yaw * RAD_TO_DEG;
}

void vector_multiply_plus(const Vector3& v1, const Vector3& v2, float coef, Vector3& out) {
    out.x = v1.x + coef * v2.x;
    out.y = v1.y + coef * v2.y;
    out.z = v1.z + coef * v2.z;
}

float vector_norm(const Vector3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

void quaternion_to_rotation_matrix(const Quaternion& q, Matrix3x3& R) {
    float q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
    R.m[0][0] = 1.0f - 2.0f * (q2 * q2 + q3 * q3);
    R.m[0][1] = 2.0f * (q1 * q2 - q0 * q3);
    R.m[0][2] = 2.0f * (q0 * q2 + q1 * q3);
    R.m[1][0] = 2.0f * (q1 * q2 + q0 * q3);
    R.m[1][1] = 1.0f - 2.0f * (q1 * q1 + q3 * q3);
    R.m[1][2] = 2.0f * (q2 * q3 - q0 * q1);
    R.m[2][0] = 2.0f * (q1 * q3 - q0 * q2);
    R.m[2][1] = 2.0f * (q0 * q1 + q2 * q3);
    R.m[2][2] = 1.0f - 2.0f * (q1 * q1 + q2 * q2);
}

void rotation_matrix_to_quaternion(const Matrix3x3& R, Quaternion& q) {
    (void)R;
    (void)q;
    // Not yet implemented
}

void quaternion_multiply(const Quaternion& q1, const Quaternion& q2, Quaternion& out) {
    out.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
    out.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
    out.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
    out.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
}

void quaternion_to_axis_angle(const Quaternion& q, Vector3& axis, float& angle) {
    float norm = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    Quaternion q_norm = {q.w / norm, q.x / norm, q.y / norm, q.z / norm};
    angle = 2.0f * acosf(q_norm.w);
    float sin_theta_over_2 = sqrtf(1.0f - q_norm.w * q_norm.w);
    if (fabsf(sin_theta_over_2) < 1e-6f) {
        axis.x = 1.0f;
        axis.y = 0.0f;
        axis.z = 0.0f;
    } else {
        axis.x = q_norm.x / sin_theta_over_2;
        axis.y = q_norm.y / sin_theta_over_2;
        axis.z = q_norm.z / sin_theta_over_2;
    }
}

void rotation_diff_quaternions(const Quaternion& q1, const Quaternion& q2, Vector3& axis, float& angle) {
    Quaternion q2_inv = {q2.w, -q2.x, -q2.y, -q2.z};
    Quaternion q;
    quaternion_multiply(q1, q2_inv, q);
    quaternion_to_axis_angle(q, axis, angle);
}

} // namespace spatial

// C-compatible wrappers
extern "C" {

void spatial_quaternion_to_euler(const Quaternion* q, Euler* e) {
    spatial::quaternion_to_euler(*q, *e);
}

void spatial_quaternion_to_euler_deg(const Quaternion* q, Euler* e_deg) {
    spatial::quaternion_to_euler_deg(*q, *e_deg);
}

void spatial_vector_multiply_plus(const Vector3* v1, const Vector3* v2, float coef, Vector3* v) {
    spatial::vector_multiply_plus(*v1, *v2, coef, *v);
}

float spatial_vector_norm(const Vector3* v) {
    return spatial::vector_norm(*v);
}

void spatial_quaternion_to_rotation_matrix(const Quaternion* q, Matrix3x3* R) {
    spatial::quaternion_to_rotation_matrix(*q, *R);
}

void spatial_rotation_matrix_to_quaternion(const Matrix3x3* R, Quaternion* q) {
    spatial::rotation_matrix_to_quaternion(*R, *q);
}

void spatial_quaternion_multiply(const Quaternion* q1, const Quaternion* q2, Quaternion* q) {
    spatial::quaternion_multiply(*q1, *q2, *q);
}

void spatial_quaternion_to_axis_angle(const Quaternion* q, Vector3* axis, float* angle) {
    spatial::quaternion_to_axis_angle(*q, *axis, *angle);
}

void spatial_rotation_diff_quaternions(const Quaternion* q1, const Quaternion* q2, Vector3* axis, float* angle) {
    spatial::rotation_diff_quaternions(*q1, *q2, *axis, *angle);
}

} // extern "C"

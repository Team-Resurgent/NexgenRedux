#pragma once

#include <Gensys/Int.h>

#include <math.h>

namespace NexgenRedux
{
	class MathUtility
	{		
	public:

		typedef struct Vec2D
		{
			union {
				struct {
					double x;
					double y;
				};
				double values[2];
			};

			Vec2D();
			Vec2D(const double x, const double y);
			Vec2D(const Vec2D& other);

			Vec2D operator/(const Vec2D& other) const;
			Vec2D operator*(const Vec2D& other) const;
			Vec2D operator+(const Vec2D& other) const; 
			Vec2D operator-(const Vec2D& other) const;

			void operator/=(const Vec2D& other);
			void operator*=(const Vec2D& other);
			void operator+=(const Vec2D& other);
			void operator-=(const Vec2D& other);
			bool operator==(const Vec2D& other) const;
			bool operator!=(const Vec2D& other) const;

		} Vec2D;

		typedef struct Vec2F
		{
			union {
				struct {
					float x;
					float y;
				};
				float values[2];
			};

			Vec2F();
			Vec2F(const float x, const float y);
			Vec2F(const Vec2F& other);

			Vec2F operator/(const Vec2F& other) const;
			Vec2F operator*(const Vec2F& other) const;
			Vec2F operator+(const Vec2F& other) const;
			Vec2F operator-(const Vec2F& other) const;

			void operator/=(const Vec2F& other);
			void operator*=(const Vec2F& other);
			void operator+=(const Vec2F& other);
			void operator-=(const Vec2F& other);
			bool operator==(const Vec2F& other) const;
			bool operator!=(const Vec2F& other) const;

		} Vec2F;

		typedef struct Vec3F
		{
			union {
				struct {
					float x;
					float y;
					float z;
				};
				float values[3];
			};

			Vec3F();
			Vec3F(const float x, const float y, const float z);
			Vec3F(const Vec3F& other);

			Vec3F operator/(const Vec3F& other) const;
			Vec3F operator*(const Vec3F& other) const;
			Vec3F operator+(const Vec3F& other) const;
			Vec3F operator-(const Vec3F& other) const;

			void operator/=(const Vec3F& other);
			void operator*=(const Vec3F& other);
			void operator+=(const Vec3F& other);
			void operator-=(const Vec3F& other);
			bool operator==(const Vec3F& other) const;
			bool operator!=(const Vec3F& other) const;

			float Dot(const Vec3F& other);
			Vec3F Cross(const Vec3F& other);
			Vec3F Normal();

		} Vec3F;

		typedef struct Vec4F
		{
			union {
				struct {
					float x;
					float y;
					float z;
					float w;
				};
				float values[4];
			};

			Vec4F();
            Vec4F(const float x, const float y, const float z, const float w);
            Vec4F(const Vec4F& other);

            Vec4F operator/(const Vec4F& other) const;
            Vec4F operator*(const Vec4F& other) const;
            Vec4F operator+(const Vec4F& other) const;
            Vec4F operator-(const Vec4F& other) const;

            void operator/=(const Vec4F& other);
            void operator*=(const Vec4F& other);
            void operator+=(const Vec4F& other);
            void operator-=(const Vec4F& other);
            bool operator==(const Vec4F& other) const;
            bool operator!=(const Vec4F& other) const;

		} Vec4F;

		typedef struct Matrix4x4
		{
			union {
				struct {
					float m11;
					float m12;
					float m13;
					float m14;
					float m21;
					float m22;
					float m23;
					float m24;
					float m31;
					float m32;
					float m33;
					float m34;
					float m41;
					float m42;
					float m43;
					float m44;
				};
				float values[16];
			};

			Matrix4x4() 
			{
				m11 = 1.0f;
				m12 = 0.0f;
				m13 = 0.0f;
				m14 = 0.0f;
				m21 = 0.0f;
				m22 = 1.0f;
				m23 = 0.0f;
				m24 = 0.0f;
				m31 = 0.0f;
				m32 = 0.0f;
				m33 = 1.0f;
				m34 = 0.0f;
				m41 = 0.0f;
				m42 = 0.0f;
				m43 = 0.0f;
				m44 = 1.0f;
			}

			Matrix4x4(const Matrix4x4& other) 
			{
				for (uint32_t i = 0; i < 16; i++)
				{
					values[i] = other.values[i];
				}
			}

			Matrix4x4 operator * (const Matrix4x4& other) const 
			{
				Matrix4x4 result;
				for (uint32_t i = 0; i < 4; i++) 
				{
					for (uint32_t j = 0; j < 4; j++) 
					{
						float sum = 0;
						for (uint32_t k = 0; k < 4; k++) 
						{
							sum += values[i * 4 + k] * other.values[k * 4 + j];
						}
						result.values[i * 4 + j] = sum;
					}
				}
				return result;
			}

			Matrix4x4 operator + (const Matrix4x4& other) const 
			{
				Matrix4x4 result;
				for (uint32_t i = 0; i < 16; ++i) 
				{
					result.values[i] = values[i] + other.values[i];
				}
				return result;
			}

			Matrix4x4 operator - (const Matrix4x4& other) const 
			{
				Matrix4x4 result;
				for (uint32_t i = 0; i < 16; ++i) 
				{
					result.values[i] = values[i] - other.values[i];
				}
				return result;
			}

			void operator *= (const Matrix4x4& other)
			{
				Matrix4x4 result = *this * other;
				for (uint32_t i = 0; i < 16; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator += (const Matrix4x4& other)
			{
				Matrix4x4 result = *this + other;
				for (uint32_t i = 0; i < 16; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Matrix4x4& other)
			{
				Matrix4x4 result = *this - other;
				for (uint32_t i = 0; i < 16; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Matrix4x4& other) const 
			{ 
				for (uint32_t i = 0; i < 16; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-6f) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Matrix4x4& other) const { return !(*this == other); }

			static Matrix4x4 Transpose(const Matrix4x4& value)
			{
				Matrix4x4 result;
				result.values[0] = value.values[0];
				result.values[1] = value.values[4];
				result.values[2] = value.values[8];
				result.values[3] = value.values[12];
				result.values[4] = value.values[1];
				result.values[5] = value.values[5];
				result.values[6] = value.values[9];
				result.values[7] = value.values[13];
				result.values[8] = value.values[2];
				result.values[9] = value.values[6];
				result.values[10] = value.values[10];
				result.values[11] = value.values[14];
				result.values[12] = value.values[3];
				result.values[13] = value.values[7];
				result.values[14] = value.values[11];
				result.values[15] = value.values[15];
				return result;
			}

			static Matrix4x4 Inverse(const Matrix4x4& value)
			{
				Matrix4x4 inverse;
				inverse.values[0] = value.values[5] * value.values[10] * value.values[15] - value.values[5] * value.values[11] * value.values[14] - value.values[9] * value.values[6] * value.values[15] + value.values[9] * value.values[7] * value.values[14] + value.values[13] * value.values[6] * value.values[11] - value.values[13] * value.values[7] * value.values[10];
				inverse.values[4] = -value.values[4] * value.values[10] * value.values[15] + value.values[4] * value.values[11] * value.values[14] + value.values[8] * value.values[6] * value.values[15] - value.values[8] * value.values[7] * value.values[14] - value.values[12] * value.values[6] * value.values[11] + value.values[12] * value.values[7] * value.values[10];
				inverse.values[8] = value.values[4] * value.values[9] * value.values[15] - value.values[4] * value.values[11] * value.values[13] - value.values[8] * value.values[5] * value.values[15] + value.values[8] * value.values[7] * value.values[13] + value.values[12] * value.values[5] * value.values[11] - value.values[12] * value.values[7] * value.values[9];
				inverse.values[12] = -value.values[4] * value.values[9] * value.values[14] + value.values[4] * value.values[10] * value.values[13] + value.values[8] * value.values[5] * value.values[14] - value.values[8] * value.values[6] * value.values[13] - value.values[12] * value.values[5] * value.values[10] + value.values[12] * value.values[6] * value.values[9];
				inverse.values[1] = -value.values[1] * value.values[10] * value.values[15] + value.values[1] * value.values[11] * value.values[14] + value.values[9] * value.values[2] * value.values[15] - value.values[9] * value.values[3] * value.values[14] - value.values[13] * value.values[2] * value.values[11] + value.values[13] * value.values[3] * value.values[10];
				inverse.values[5] = value.values[0] * value.values[10] * value.values[15] - value.values[0] * value.values[11] * value.values[14] - value.values[8] * value.values[2] * value.values[15] + value.values[8] * value.values[3] * value.values[14] + value.values[12] * value.values[2] * value.values[11] - value.values[12] * value.values[3] * value.values[10];
				inverse.values[9] = -value.values[0] * value.values[9] * value.values[15] + value.values[0] * value.values[11] * value.values[13] + value.values[8] * value.values[1] * value.values[15] - value.values[8] * value.values[3] * value.values[13] - value.values[12] * value.values[1] * value.values[11] + value.values[12] * value.values[3] * value.values[9];
				inverse.values[13] = value.values[0] * value.values[9] * value.values[14] - value.values[0] * value.values[10] * value.values[13] - value.values[8] * value.values[1] * value.values[14] + value.values[8] * value.values[2] * value.values[13] + value.values[12] * value.values[1] * value.values[10] - value.values[12] * value.values[2] * value.values[9];
				inverse.values[2] = value.values[1] * value.values[6] * value.values[15] - value.values[1] * value.values[7] * value.values[14] - value.values[5] * value.values[2] * value.values[15] + value.values[5] * value.values[3] * value.values[14] + value.values[13] * value.values[2] * value.values[7] - value.values[13] * value.values[3] * value.values[6];
				inverse.values[6] = -value.values[0] * value.values[6] * value.values[15] + value.values[0] * value.values[7] * value.values[14] + value.values[4] * value.values[2] * value.values[15] - value.values[4] * value.values[3] * value.values[14] - value.values[12] * value.values[2] * value.values[7] + value.values[12] * value.values[3] * value.values[6];
				inverse.values[10] = value.values[0] * value.values[5] * value.values[15] - value.values[0] * value.values[7] * value.values[13] - value.values[4] * value.values[1] * value.values[15] + value.values[4] * value.values[3] * value.values[13] + value.values[12] * value.values[1] * value.values[7] - value.values[12] * value.values[3] * value.values[5];
				inverse.values[14] = -value.values[0] * value.values[5] * value.values[14] + value.values[0] * value.values[6] * value.values[13] + value.values[4] * value.values[1] * value.values[14] - value.values[4] * value.values[2] * value.values[13] - value.values[12] * value.values[1] * value.values[6] + value.values[12] * value.values[2] * value.values[5];
				inverse.values[3] = -value.values[1] * value.values[6] * value.values[11] + value.values[1] * value.values[7] * value.values[10] + value.values[5] * value.values[2] * value.values[11] - value.values[5] * value.values[3] * value.values[10] - value.values[9] * value.values[2] * value.values[7] + value.values[9] * value.values[3] * value.values[6];
				inverse.values[7] = value.values[0] * value.values[6] * value.values[11] - value.values[0] * value.values[7] * value.values[10] - value.values[4] * value.values[2] * value.values[11] + value.values[4] * value.values[3] * value.values[10] + value.values[8] * value.values[2] * value.values[7] - value.values[8] * value.values[3] * value.values[6];
				inverse.values[11] = -value.values[0] * value.values[5] * value.values[11] + value.values[0] * value.values[7] * value.values[9] + value.values[4] * value.values[1] * value.values[11] - value.values[4] * value.values[3] * value.values[9] - value.values[8] * value.values[1] * value.values[7] + value.values[8] * value.values[3] * value.values[5];
				inverse.values[15] = value.values[0] * value.values[5] * value.values[10] - value.values[0] * value.values[6] * value.values[9] - value.values[4] * value.values[1] * value.values[10] + value.values[4] * value.values[2] * value.values[9] + value.values[8] * value.values[1] * value.values[6] - value.values[8] * value.values[2] * value.values[5];
				
				float det = 1.0f / (float)(value.values[0] * inverse.values[0] + value.values[1] * inverse.values[4] + value.values[2] * inverse.values[8] + value.values[3] * inverse.values[12]);
				
				Matrix4x4 result;
				for (uint32_t i = 0; i < 16; i++)
				{
					result.values[i] = inverse.values[i] * det;
				}
				return result;
			}

			static Matrix4x4 Scale(const Vec3F& value)
			{
				Matrix4x4 result = Matrix4x4();
				result.values[0] = value.values[0];
				result.values[5] = value.values[1];
				result.values[10] = value.values[2];
				return result;
			}

			static Matrix4x4 RotateX(const float& degreesX)
			{
				Matrix4x4 result = Matrix4x4();
				result.values[5] = cosf(DegreesToRadians(degreesX));
				result.values[6] = sinf(DegreesToRadians(degreesX));
				result.values[9] = -result.values[6];
				result.values[10] = result.values[5];
				return result;
			}

			static Matrix4x4 RotateY(const float& degreesY)
			{
				Matrix4x4 result = Matrix4x4();
				result.values[0] = cosf(DegreesToRadians(degreesY));
				result.values[2] = -sinf(DegreesToRadians(degreesY));
				result.values[8] = -result.values[2];
				result.values[10] = result.values[0];
				return result;
			}

			static Matrix4x4 RotateZ(const float& degreesZ)
			{
				Matrix4x4 result = Matrix4x4();
				result.values[0] = cosf(DegreesToRadians(degreesZ));
				result.values[1] = sinf(DegreesToRadians(degreesZ));
				result.values[4] = -result.values[1];
				result.values[5] = result.values[0];
				return result;
			}

			static Matrix4x4 Rotation(Vec3F rotation) 
			{
				float cosX = cosf(DegreesToRadians(rotation.values[0]));
				float sinX = sinf(DegreesToRadians(rotation.values[0]));
				float cosY = cosf(DegreesToRadians(rotation.values[1]));
				float sinY = sinf(DegreesToRadians(rotation.values[1]));
				float cosZ = cosf(DegreesToRadians(rotation.values[2]));
				float sinZ = sinf(DegreesToRadians(rotation.values[2]));
				
				Matrix4x4 result = Matrix4x4();
				result.values[0] = cosY * cosZ;
				result.values[1] = cosX * sinZ + sinX * sinY * cosZ;
				result.values[2] = sinX * sinZ - cosX * sinY * cosZ;
				result.values[4] = -cosY * sinZ;
				result.values[5] = cosX * cosZ - sinX * sinY * sinZ;
				result.values[6] = sinX * cosZ + cosX * sinY * sinZ;
				result.values[8] = sinY;
				result.values[9] = -sinX * cosY;
				result.values[10] = cosX * cosY;
				return result;
			}

			static Matrix4x4 Translate(const Vec3F& value)
			{
				Matrix4x4 result = Matrix4x4();
				result.values[12] = value.values[0];
				result.values[13] = value.values[1];
				result.values[14] = value.values[2];
				return result;
			}

			static Matrix4x4 Skew(const Vec3F& value)
			{
				Matrix4x4 result = Matrix4x4();
				result.values[1] = value.values[0];
				result.values[4] = value.values[1];
				result.values[8] = value.values[2];
				return result;
			}

			static Matrix4x4 LookAtRH(const Vec3F& eye, const Vec3F& target, const Vec3F& up)
			{
				Vec3F zaxis = eye - target;
				zaxis = zaxis.Normal();
				
				Vec3F xaxis = up;
				xaxis = xaxis.Cross(zaxis);
				xaxis = xaxis.Normal();

				Vec3F yaxis = zaxis.Cross(xaxis);

				Matrix4x4 result;
				result.values[0] = xaxis.values[0];
				result.values[1] = yaxis.values[0];
				result.values[2] = zaxis.values[0];
				result.values[3] = 0;
				result.values[4] = xaxis.values[1];
				result.values[5] = yaxis.values[1];
				result.values[6] = zaxis.values[1];
				result.values[7] = 0;
				result.values[8] = xaxis.values[2];
				result.values[9] = yaxis.values[2];
				result.values[10] = zaxis.values[2];
				result.values[11] = 0;
				result.values[12] = -xaxis.Dot(eye);
				result.values[13] = -yaxis.Dot(eye);
				result.values[14] = -zaxis.Dot(eye);
				result.values[15] = 1;
				return result;
			}
					
			static Matrix4x4 LookAtLH(const Vec3F& eye, const Vec3F& target, const Vec3F& up)
			{
				Vec3F zaxis = target - eye;
				zaxis = zaxis.Normal();
				
				Vec3F xaxis = up;
				xaxis = xaxis.Cross(zaxis);
				xaxis = xaxis.Normal();

				Vec3F yaxis = zaxis.Cross(xaxis);

				Matrix4x4 result;
				result.values[0] = xaxis.values[0];
				result.values[1] = yaxis.values[0];
				result.values[2] = zaxis.values[0];
				result.values[3] = 0;
				result.values[4] = xaxis.values[1];
				result.values[5] = yaxis.values[1];
				result.values[6] = zaxis.values[1];
				result.values[7] = 0;
				result.values[8] = xaxis.values[2];
				result.values[9] = yaxis.values[2];
				result.values[10] = zaxis.values[2];
				result.values[11] = 0;
				result.values[12] = -xaxis.Dot(eye);
				result.values[13] = -yaxis.Dot(eye);
				result.values[14] = -zaxis.Dot(eye);
				result.values[15] = 1;
				return result;
			}

			static Matrix4x4 LookAt(const Vec3F& eye, const Vec3F& target, const Vec3F& up)
			{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
				return LookAtRH(eye, target, up);
#else
				return LookAtLH(eye, target, up);
#endif
			}
					
			static Matrix4x4 PerspectiveFovRH(const float& fovDegrees, const float& aspectRatio, const float& znear, const float& zfar)
			{
				float sin = sinf(0.5f * DegreesToRadians(fovDegrees));
				float cos = cosf(0.5f * DegreesToRadians(fovDegrees));
				float h = cos / sin;
				float w = h / aspectRatio;
				
				Matrix4x4 result;
				result.values[0] = w;
				result.values[1] = 0.0f;
				result.values[2] = 0.0f;
				result.values[3] = 0.0f;
				result.values[4] = 0.0f;
				result.values[5] = h;
				result.values[6] = 0.0f;
				result.values[7] = 0.0f;
				result.values[8] = 0.0f;
				result.values[9] = 0.0f;
				result.values[10] = zfar / (znear - zfar);
				result.values[11] = -1.0f;
				result.values[12] = 0.0f;
				result.values[13] = 0.0f;
				result.values[14] = result.values[10] * znear;
				result.values[15] = 0.0f;
				return result;
			}

			static Matrix4x4 PerspectiveFovLH(const float& fovDegrees, const float& aspectRatio, const float& znear, const float& zfar)
			{
				float sin = sinf(0.5f * DegreesToRadians(fovDegrees));
				float cos = cosf(0.5f * DegreesToRadians(fovDegrees));
				float h = cos / sin;
				float w = h / aspectRatio;
				
				Matrix4x4 result;
				result.values[0] = w;
				result.values[1] = 0.0f;
				result.values[2] = 0.0f;
				result.values[3] = 0.0f;
				result.values[4] = 0.0f;
				result.values[5] = h;
				result.values[6] = 0.0f;
				result.values[7] = 0.0f;
				result.values[8] = 0.0f;
				result.values[9] = 0.0f;
				result.values[10] = zfar / (zfar - znear);
				result.values[11] = 1.0f;
				result.values[12] = 0.0f;
				result.values[13] = 0.0f;
				result.values[14] = -result.values[10] * znear;
				result.values[15] = 0.0f;
				return result;
			}

			static Matrix4x4 PerspectiveFov(const float& fovDegrees, const float& aspectRatio, const float& znear, const float& zfar)
			{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
				return PerspectiveFovRH(fovDegrees, aspectRatio, znear, zfar);
#else
				return PerspectiveFovLH(fovDegrees, aspectRatio, znear, zfar);
#endif
			}

			static Matrix4x4 OrthoRH(const float& width, const float& height, const float& znear, const float& zfar)
			{
				Matrix4x4 result;
				result.values[0] = 2.0f / width;
				result.values[1] = 0.0f;
				result.values[2] = 0.0f;
				result.values[3] = 0.0f;
				result.values[4] = 0.0f;
				result.values[5] = 2.0f / height;
				result.values[6] = 0.0f;
				result.values[7] = 0.0f;
				result.values[8] = 0.0f;			
				result.values[9] = 0.0f;
				result.values[10] = 1.0f / (znear - zfar);
				result.values[11] = 0.0f;
				result.values[12] = 0.0f;
				result.values[13] = 0.0f;
				result.values[14] = -result.values[10] * znear;
				result.values[15] = 1.0f;
				return result;
			}

			static Matrix4x4 OrthoLH(const float& width, const float& height, const float& znear, const float& zfar)
			{
				Matrix4x4 result;
				result.values[0] = 2.0f / width;
				result.values[1] = 0.0f;
				result.values[2] = 0.0f;
				result.values[3] = 0.0f;
				result.values[4] = 0.0f;
				result.values[5] = 2.0f / height;
				result.values[6] = 0.0f;
				result.values[7] = 0.0f;
				result.values[8] = 0.0f;
				result.values[9] = 0.0f;
				result.values[10] = 1.0f / (zfar - znear);
				result.values[11] = 0.0f;
				result.values[12] = 0.0f;
				result.values[13] = 0.0f;
				result.values[14] = -result.values[10] * znear;
				result.values[15] = 1.0f;
				return result;
			}

			static Matrix4x4 Ortho(const float& width, const float& height, const float& znear, const float& zfar)
			{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
				return OrthoRH(width, height, znear, zfar);
#else
				return OrthoLH(width, height, znear, zfar);
#endif
			}

			static Matrix4x4 OrthoOffCenterRH(const float& left, const float& right, const float& bottom, const float& top, const float& znear, const float& zfar)
			{
				float wInv = 1.0f / (right - left);
				float hInv = 1.0f / (top - bottom);

				Matrix4x4 result;
				result.values[0] = 2.0f * wInv;
				result.values[1] = 0.0f;
				result.values[2] = 0.0f;
				result.values[3] = 0.0f;
				result.values[4] = 0.0f;
				result.values[5] = 2.0f * hInv;
				result.values[6] = 0.0f;
				result.values[7] = 0.0f;
				result.values[8] = 0.0f;			
				result.values[9] = 0.0f;
				result.values[10] = 1.0f / (znear - zfar);
				result.values[11] = 0.0f;
				result.values[12] = -(left + right) * wInv;
				result.values[13] = -(top + bottom) * hInv;
				result.values[14] = result.values[10] * znear;;
				result.values[15] = 1.0f;
				return result;
			}

			static Matrix4x4 OrthoOffCenterLH(const float& left, const float& right, const float& bottom, const float& top, const float& znear, const float& zfar)
			{
				float wInv = 1.0f / (right - left);
				float hInv = 1.0f / (top - bottom);

				Matrix4x4 result;
				result.values[0] = 2.0f * wInv;
				result.values[1] = 0.0f;
				result.values[2] = 0.0f;
				result.values[3] = 0.0f;
				result.values[4] = 0.0f;
				result.values[5] = 2.0f * hInv;
				result.values[6] = 0.0f;
				result.values[7] = 0.0f;
				result.values[8] = 0.0f;
				result.values[9] = 0.0f;
				result.values[10] = 1.0f / (zfar - znear);
				result.values[11] = 0.0f;
				result.values[12] = -(left + right) * wInv;
				result.values[13] = -(top + bottom) * hInv;
				result.values[14] = -result.values[10] * znear;;
				result.values[15] = 1.0f;
				return result;
			}

			static Matrix4x4 OrthoOffCenter(const float& left, const float& right, const float& bottom, const float& top, const float& znear, const float& zfar)
			{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
				return OrthoOffCenterRH(left, right, bottom, top, znear, zfar);
#else
				return OrthoOffCenterLH(left, right, bottom, top, znear, zfar);
#endif
			}

			static Vec3F GetPosition(const Matrix4x4& matrix)
			{
				return Vec3F(matrix.values[12] / matrix.values[15], matrix.values[13] / matrix.values[15], matrix.values[14] / matrix.values[15]);
			}

			static Vec4F MultiplyMatrixByVec4F(const Matrix4x4& matrix, const Vec4F& vec)
			{
				Vec4F result = Vec4F();
				for (uint32_t i = 0; i < 4; i++) 
				{
					result.values[i] = 0;
					for (uint32_t j = 0; j < 4; j++) 
					{
						result.values[i] += matrix.values[i * 4 + j] * vec.values[j];
					}
				}
				return result;
			}


			// static Vec3F TransformVec3F(const Matrix4x4& matrix, const Vec3F& vec)
			// {
			// 	Vec4F result = Vec4F(
			// 		matrix.values[0] * vec.x + matrix.values[1] * vec.y + matrix.values[2] * vec.z + matrix.values[3],
			// 		matrix.values[4] * vec.x + matrix.values[5] * vec.y + matrix.values[6] * vec.z + matrix.values[7],
			// 		matrix.values[8] * vec.x + matrix.values[9] * vec.y + matrix.values[10] * vec.z + matrix.values[11],
			// 		matrix.values[12] * vec.x + matrix.values[13] * vec.y + matrix.values[14] * vec.z + matrix.values[15]
			// 	);

			// 	if (result.w != 0)
			// 	{
			// 		return Vec3F(result.x / result.w, result.y / result.w, result.z / result.w);
			// 	}

			// 	return Vec3F(result.x, result.y, result.z);
			// }

		} Matrix4x4;

		typedef struct SizeI
		{
			union {
				struct {
					int32_t width;
					int32_t height;
				};
				int32_t values[2];
			};

			SizeI();
			SizeI(const int32_t width, const int32_t height);
			SizeI(const SizeI& other);

			bool operator == (const SizeI& other) const;
			bool operator != (const SizeI& other) const;

		} SizeI;

		typedef struct SizeF
		{
			union {
				struct {
					float width;
					float height;
				};
				float values[2];
			};

			SizeF();
			SizeF(const float width, const float height);
			SizeF(const SizeF& other);

			bool operator == (const SizeF& other) const;
			bool operator != (const SizeF& other) const;

		} SizeF;

		struct RectI
		{
			union {
				struct {
					int32_t x;
					int32_t y;
					int32_t width;
					int32_t height;
				};
				int32_t values[4];
			};

			RectI();
			RectI(const int32_t x, const int32_t y, const int32_t width, const int32_t height);
			RectI(const RectI& other);

			bool operator==(const RectI& other) const;
			bool operator!=(const RectI& other) const;

			bool ContainsPoint(const int32_t& x, const int32_t& y) const;

		};

		struct RectF
		{
			union {
				struct {
					float x;
					float y;
					float width;
					float height;
				};
				float values[4];
			};

			RectF(); 
			RectF(const float x, const float y, const float width, const float height);
			RectF(const RectF& other);

			bool operator==(const RectF& other) const;
			bool operator!=(const RectF& other) const;

			bool ContainsPoint(const float& x, const float& y) const;

		};

		struct Color3F
		{
			union {
				struct {
					float r;
					float g;
					float b;
				};
				float values[3];
			};

			Color3F();
			Color3F(const float r, const float g, const float b);
			Color3F(const Color3F& other);

			Color3F operator/(const Color3F& other) const;
			Color3F operator*(const Color3F& other) const;
			Color3F operator+(const Color3F& other) const;
			Color3F operator-(const Color3F& other) const;

			void operator/=(const Color3F& other);
			void operator*=(const Color3F& other);
			void operator+=(const Color3F& other);
			void operator-=(const Color3F& other);
			bool operator == (const Color3F& other) const;
			bool operator != (const Color3F& other) const;

		};

		struct Color4F
		{
			union {
				struct {
					float r;
					float g;
					float b;
					float a;
				};
				float values[4];
			};

			Color4F();
			Color4F(const float r, const float g, const float b, const float a);
			Color4F(const Color4F& other);

			Color4F operator/(const Color4F& other) const;
			Color4F operator*(const Color4F& other) const;
			Color4F operator+(const Color4F& other) const;
			Color4F operator-(const Color4F& other) const;

			void operator/=(const Color4F& other);
			void operator*=(const Color4F& other);
			void operator+=(const Color4F& other);
			void operator-=(const Color4F& other);
			bool operator == (const Color4F& other) const;
			bool operator != (const Color4F& other) const;

		};

		static float PI();
		static float DegreesToRadians(float const degrees);

		static void SeedRandom();
		static void SeedRandomWithValue(uint32_t value);
		static double GetRandomDouble();
		static int32_t GetRandomIntInRange(int32_t start, int32_t end);

		static void ConvertEndian(uint32_t& value);
		static void ConvertEndian(uint16_t& value);
		static void ConvertEndian(float& value); 
		static bool IsPowerOf2(uint32_t value);
	};
}

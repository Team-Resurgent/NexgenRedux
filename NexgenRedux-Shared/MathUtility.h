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

			Vec2D() : x(0), y(0) {} 
			Vec2D(const double& x, const double& y) : x(x), y(y) {}  

			Vec2D(const Vec2D& other) 
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					values[i] = other.values[i];
				}
			}

			Vec2D operator / (const Vec2D& other) const { return Vec2D(x / other.x, y / other.y); }  
			Vec2D operator * (const Vec2D& other) const { return Vec2D(x * other.x, y * other.y); }  
			Vec2D operator + (const Vec2D& other) const { return Vec2D(x + other.x, y + other.y); }  
			Vec2D operator - (const Vec2D& other) const { return Vec2D(x - other.x, y - other.y); }

			void operator /= (const Vec2D& other)
			{
				Vec2D result = *this / other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const Vec2D& other)
			{
				Vec2D result = *this * other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const Vec2D& other)
			{
				Vec2D result = *this + other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Vec2D& other)
			{
				Vec2D result = *this - other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Vec2D& other) const 
			{ 
				for (uint32_t i = 0; i < 2; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-9) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Vec2D& other) const { return !(*this == other); }

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

			Vec2F() : x(0), y(0) {}  
			Vec2F(const float& x, const float& y) : x(x), y(y) {}  

			Vec2F(const Vec2F& other) 
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					values[i] = other.values[i];
				}
			}

			Vec2F operator / (const Vec2F& other) const { return Vec2F(x / other.x, y / other.y); }  
			Vec2F operator * (const Vec2F& other) const { return Vec2F(x * other.x, y * other.y); }  
			Vec2F operator + (const Vec2F& other) const { return Vec2F(x + other.x, y + other.y); }  
			Vec2F operator - (const Vec2F& other) const { return Vec2F(x - other.x, y - other.y); }

			void operator /= (const Vec2F& other)
			{
				Vec2F result = *this / other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const Vec2F& other)
			{
				Vec2F result = *this * other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const Vec2F& other)
			{
				Vec2F result = *this + other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Vec2F& other)
			{
				Vec2F result = *this - other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Vec2F& other) const 
			{ 
				for (uint32_t i = 0; i < 2; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-6f) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Vec2F& other) const { return !(*this == other); }

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

			Vec3F() : x(0), y(0), z(0) {}  
			Vec3F(const float& x, const float& y, const float& z) : x(x), y(y), z(z) {}  

			Vec3F(const Vec3F& other) 
			{
				for (uint32_t i = 0; i < 3; i++)
				{
					values[i] = other.values[i];
				}
			}

			Vec3F operator / (const Vec3F& other) const { return Vec3F(x / other.x, y / other.y, z / other.z); }  
			Vec3F operator * (const Vec3F& other) const { return Vec3F(x * other.x, y * other.y, z * other.z); }  
			Vec3F operator + (const Vec3F& other) const { return Vec3F(x + other.x, y + other.y, z + other.z); }  
			Vec3F operator - (const Vec3F& other) const { return Vec3F(x - other.x, y - other.y, z - other.z); }

			void operator /= (const Vec3F& other)
			{
				Vec3F result = *this / other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const Vec3F& other)
			{
				Vec3F result = *this * other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const Vec3F& other)
			{
				Vec3F result = *this + other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Vec3F& other)
			{
				Vec3F result = *this - other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Vec3F& other) const 
			{ 
				for (uint32_t i = 0; i < 3; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-6f) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Vec3F& other) const { return !(*this == other); }

			float Dot(const Vec3F& other) 
			{ 
				return x * other.x + y * other.y + z * other.z; 
			}

			Vec3F Cross(const Vec3F& other) 
			{ 
				Vec3F result;
				result.values[0] = values[1] * other.values[2] - values[2] * other.values[1];
				result.values[1] = values[2] * other.values[0] - values[0] * other.values[2];
				result.values[2] = values[0] * other.values[1] - values[1] * other.values[0];		
				return result; 
			}

			Vec3F Normal()
			{
				float length = (float)sqrt(x * x + y * y + z * z);
				return Vec3F(x / length, y / length, z / length);
			}

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

			Vec4F() : x(0), y(0), z(0), w(0) {}  
			Vec4F(const float& x, const float& y, const float& z, const float& w) : x(x), y(y), z(z), w(w) {}  

			Vec4F(const Vec4F& other) 
			{
				for (uint32_t i = 0; i < 4; i++)
				{
					values[i] = other.values[i];
				}
			}

			Vec4F operator / (const Vec4F& other) const { return Vec4F(x / other.x, y / other.y, z / other.z, w / other.w); }  
			Vec4F operator * (const Vec4F& other) const { return Vec4F(x * other.x, y * other.y, z * other.z, w * other.w); }  
			Vec4F operator + (const Vec4F& other) const { return Vec4F(x + other.x, y + other.y, z + other.z, w + other.w); }  
			Vec4F operator - (const Vec4F& other) const { return Vec4F(x - other.x, y - other.y, z - other.z, w - other.w); }

			void operator /= (const Vec4F& other)
			{
				Vec4F result = *this / other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const Vec4F& other)
			{
				Vec4F result = *this * other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const Vec4F& other)
			{
				Vec4F result = *this + other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Vec4F& other)
			{
				Vec4F result = *this - other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Vec4F& other) const 
			{ 
				for (uint32_t i = 0; i < 4; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-6f) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Vec4F& other) const { return !(*this == other); }

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

			Matrix4x4 Transpose(const Matrix4x4& other)
			{
				Matrix4x4 result;
				result.values[0] = other.values[0];
				result.values[1] = other.values[4];
				result.values[2] = other.values[8];
				result.values[3] = other.values[12];
				result.values[4] = other.values[1];
				result.values[5] = other.values[5];
				result.values[6] = other.values[9];
				result.values[7] = other.values[13];
				result.values[8] = other.values[2];
				result.values[9] = other.values[6];
				result.values[10] = other.values[10];
				result.values[11] = other.values[14];
				result.values[12] = other.values[3];
				result.values[13] = other.values[7];
				result.values[14] = other.values[11];
				result.values[15] = other.values[15];
				return result;
			}

			Matrix4x4 Invert(const Matrix4x4& other)
			{
				Matrix4x4 inverse;
				inverse.values[0] = other.values[5] * other.values[10] * other.values[15] - other.values[5] * other.values[11] * other.values[14] - other.values[9] * other.values[6] * other.values[15] + other.values[9] * other.values[7] * other.values[14] + other.values[13] * other.values[6] * other.values[11] - other.values[13] * other.values[7] * other.values[10];
				inverse.values[4] = -other.values[4] * other.values[10] * other.values[15] + other.values[4] * other.values[11] * other.values[14] + other.values[8] * other.values[6] * other.values[15] - other.values[8] * other.values[7] * other.values[14] - other.values[12] * other.values[6] * other.values[11] + other.values[12] * other.values[7] * other.values[10];
				inverse.values[8] = other.values[4] * other.values[9] * other.values[15] - other.values[4] * other.values[11] * other.values[13] - other.values[8] * other.values[5] * other.values[15] + other.values[8] * other.values[7] * other.values[13] + other.values[12] * other.values[5] * other.values[11] - other.values[12] * other.values[7] * other.values[9];
				inverse.values[12] = -other.values[4] * other.values[9] * other.values[14] + other.values[4] * other.values[10] * other.values[13] + other.values[8] * other.values[5] * other.values[14] - other.values[8] * other.values[6] * other.values[13] - other.values[12] * other.values[5] * other.values[10] + other.values[12] * other.values[6] * other.values[9];
				inverse.values[1] = -other.values[1] * other.values[10] * other.values[15] + other.values[1] * other.values[11] * other.values[14] + other.values[9] * other.values[2] * other.values[15] - other.values[9] * other.values[3] * other.values[14] - other.values[13] * other.values[2] * other.values[11] + other.values[13] * other.values[3] * other.values[10];
				inverse.values[5] = other.values[0] * other.values[10] * other.values[15] - other.values[0] * other.values[11] * other.values[14] - other.values[8] * other.values[2] * other.values[15] + other.values[8] * other.values[3] * other.values[14] + other.values[12] * other.values[2] * other.values[11] - other.values[12] * other.values[3] * other.values[10];
				inverse.values[9] = -other.values[0] * other.values[9] * other.values[15] + other.values[0] * other.values[11] * other.values[13] + other.values[8] * other.values[1] * other.values[15] - other.values[8] * other.values[3] * other.values[13] - other.values[12] * other.values[1] * other.values[11] + other.values[12] * other.values[3] * other.values[9];
				inverse.values[13] = other.values[0] * other.values[9] * other.values[14] - other.values[0] * other.values[10] * other.values[13] - other.values[8] * other.values[1] * other.values[14] + other.values[8] * other.values[2] * other.values[13] + other.values[12] * other.values[1] * other.values[10] - other.values[12] * other.values[2] * other.values[9];
				inverse.values[2] = other.values[1] * other.values[6] * other.values[15] - other.values[1] * other.values[7] * other.values[14] - other.values[5] * other.values[2] * other.values[15] + other.values[5] * other.values[3] * other.values[14] + other.values[13] * other.values[2] * other.values[7] - other.values[13] * other.values[3] * other.values[6];
				inverse.values[6] = -other.values[0] * other.values[6] * other.values[15] + other.values[0] * other.values[7] * other.values[14] + other.values[4] * other.values[2] * other.values[15] - other.values[4] * other.values[3] * other.values[14] - other.values[12] * other.values[2] * other.values[7] + other.values[12] * other.values[3] * other.values[6];
				inverse.values[10] = other.values[0] * other.values[5] * other.values[15] - other.values[0] * other.values[7] * other.values[13] - other.values[4] * other.values[1] * other.values[15] + other.values[4] * other.values[3] * other.values[13] + other.values[12] * other.values[1] * other.values[7] - other.values[12] * other.values[3] * other.values[5];
				inverse.values[14] = -other.values[0] * other.values[5] * other.values[14] + other.values[0] * other.values[6] * other.values[13] + other.values[4] * other.values[1] * other.values[14] - other.values[4] * other.values[2] * other.values[13] - other.values[12] * other.values[1] * other.values[6] + other.values[12] * other.values[2] * other.values[5];
				inverse.values[3] = -other.values[1] * other.values[6] * other.values[11] + other.values[1] * other.values[7] * other.values[10] + other.values[5] * other.values[2] * other.values[11] - other.values[5] * other.values[3] * other.values[10] - other.values[9] * other.values[2] * other.values[7] + other.values[9] * other.values[3] * other.values[6];
				inverse.values[7] = other.values[0] * other.values[6] * other.values[11] - other.values[0] * other.values[7] * other.values[10] - other.values[4] * other.values[2] * other.values[11] + other.values[4] * other.values[3] * other.values[10] + other.values[8] * other.values[2] * other.values[7] - other.values[8] * other.values[3] * other.values[6];
				inverse.values[11] = -other.values[0] * other.values[5] * other.values[11] + other.values[0] * other.values[7] * other.values[9] + other.values[4] * other.values[1] * other.values[11] - other.values[4] * other.values[3] * other.values[9] - other.values[8] * other.values[1] * other.values[7] + other.values[8] * other.values[3] * other.values[5];
				inverse.values[15] = other.values[0] * other.values[5] * other.values[10] - other.values[0] * other.values[6] * other.values[9] - other.values[4] * other.values[1] * other.values[10] + other.values[4] * other.values[2] * other.values[9] + other.values[8] * other.values[1] * other.values[6] - other.values[8] * other.values[2] * other.values[5];
				
				float det = 1.0f / (float)(other.values[0] * inverse.values[0] + other.values[1] * inverse.values[4] + other.values[2] * inverse.values[8] + other.values[3] * inverse.values[12]);
				
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
				float cosX = cos(DegreesToRadians(rotation.values[0]));
				float sinX = sin(DegreesToRadians(rotation.values[0]));
				float cosY = cos(DegreesToRadians(rotation.values[1]));
				float sinY = sin(DegreesToRadians(rotation.values[1]));
				float cosZ = cos(DegreesToRadians(rotation.values[2]));
				float sinZ = sin(DegreesToRadians(rotation.values[2]));
				
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
				result.values[14] = result.values[10] * znear;
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
				result.values[14] = -result.values[10] * znear;
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

			SizeI() : width(0), height(0) {}  
			SizeI(const int32_t& width, const int32_t& height) : width(width), height(height) {}  

			SizeI(const SizeI& other) 
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					values[i] = other.values[i];
				}
			}

			SizeI operator / (const SizeI& other) const { return SizeI(width / other.width, height / other.height); }  
			SizeI operator * (const SizeI& other) const { return SizeI(width * other.width, height * other.height); }  
			SizeI operator + (const SizeI& other) const { return SizeI(width + other.width, height + other.height); }  
			SizeI operator - (const SizeI& other) const { return SizeI(width - other.width, height - other.height); }

			void operator /= (const SizeI& other)
			{
				SizeI result = *this / other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const SizeI& other)
			{
				SizeI result = *this * other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const SizeI& other)
			{
				SizeI result = *this + other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const SizeI& other)
			{
				SizeI result = *this - other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const SizeI& other) const 
			{ 
				for (uint32_t i = 0; i < 2; i++) 
				{
					if (values[i] != other.values[i]) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const SizeI& other) const { return !(*this == other); }

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

			SizeF() : width(0), height(0) {}  
			SizeF(const float& width, const float& height) : width(width), height(height) {}  

			SizeF(const SizeF& other) 
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					values[i] = other.values[i];
				}
			}

			SizeF operator / (const SizeF& other) const { return SizeF(width / other.width, height / other.height); }  
			SizeF operator * (const SizeF& other) const { return SizeF(width * other.width, height * other.height); }  
			SizeF operator + (const SizeF& other) const { return SizeF(width + other.width, height + other.height); }  
			SizeF operator - (const SizeF& other) const { return SizeF(width - other.width, height - other.height); }

			void operator /= (const SizeF& other)
			{
				SizeF result = *this / other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const SizeF& other)
			{
				SizeF result = *this * other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const SizeF& other)
			{
				SizeF result = *this + other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const SizeF& other)
			{
				SizeF result = *this - other;
				for (uint32_t i = 0; i < 2; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const SizeF& other) const 
			{ 
				for (uint32_t i = 0; i < 2; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-6f) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const SizeF& other) const { return !(*this == other); }

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

			RectI() : x(0), y(0), width(0), height(0) {} 
			RectI(const int32_t& x, const int32_t& y, const int32_t& width, const int32_t& height) : x(x), y(y), width(width), height(height) {}  

			RectI(const RectI& other) 
			{
				for (uint32_t i = 0; i < 4; i++)
				{
					values[i] = other.values[i];
				}
			} 

			RectI operator / (const RectI& other) const { return RectI(x / other.x, y / other.y, width / other.width, height / other.height); }  
			RectI operator * (const RectI& other) const { return RectI(x * other.x, y * other.y, width / other.width, height / other.height); }  
			RectI operator + (const RectI& other) const { return RectI(x + other.x, y + other.y, width / other.width, height / other.height); }  
			RectI operator - (const RectI& other) const { return RectI(x - other.x, y - other.y, width / other.width, height / other.height); }
			
			void operator /= (const RectI& other)
			{
				RectI result = *this / other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const RectI& other)
			{
				RectI result = *this * other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const RectI& other)
			{
				RectI result = *this + other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const RectI& other)
			{
				RectI result = *this - other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const RectI& other) const 
			{ 
				for (uint32_t i = 0; i < 4; i++) 
				{
					if (values[i] != other.values[i]) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const RectI& other) const { return !(*this == other); }

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

			RectF() : x(0), y(0), width(0), height(0) {}  
			RectF(const float& x, const float& y, const float& width, const float& height) : x(x), y(y), width(width), height(height) {}  

			RectF(const RectF& other) 
			{
				for (uint32_t i = 0; i < 4; i++)
				{
					values[i] = other.values[i];
				}
			} 

			RectF operator / (const RectF& other) const { return RectF(x / other.x, y / other.y, width / other.width, height / other.height); }  
			RectF operator * (const RectF& other) const { return RectF(x * other.x, y * other.y, width / other.width, height / other.height); }  
			RectF operator + (const RectF& other) const { return RectF(x + other.x, y + other.y, width / other.width, height / other.height); }  
			RectF operator - (const RectF& other) const { return RectF(x - other.x, y - other.y, width / other.width, height / other.height); }
			
			void operator /= (const RectF& other)
			{
				RectF result = *this / other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const RectF& other)
			{
				RectF result = *this * other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const RectF& other)
			{
				RectF result = *this + other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const RectF& other)
			{
				RectF result = *this - other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const RectF& other) const 
			{ 
				for (uint32_t i = 0; i < 4; i++) 
				{
					if (fabs(values[i] - other.values[i]) > 1e-6f) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const RectF& other) const { return !(*this == other); }

		};

		struct Color3I
		{
			union {
				struct {
					uint8_t r;
					uint8_t g;
					uint8_t b;
				};
				uint8_t values[3];
			};

			Color3I() : r(0), g(0), b(0) {}  
			Color3I(const uint8_t& r, const uint8_t& g, const uint8_t& b) : r(r), g(g), b(b) {}  

			Color3I(const Color3I& other) 
			{
				for (uint32_t i = 0; i < 3; i++)
				{
					values[i] = other.values[i];
				}
			} 

			Color3I operator / (const Color3I& other) const { return Color3I(r / other.r, g / other.g, b / b); }  
			Color3I operator * (const Color3I& other) const { return Color3I(r * other.r, g * other.g, b * b); }  
			Color3I operator + (const Color3I& other) const { return Color3I(r + other.r, g + other.g, b + b); }  
			Color3I operator - (const Color3I& other) const { return Color3I(r - other.r, g - other.g, b - b); }

			void operator /= (const Color3I& other)
			{
				Color3I result = *this / other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const Color3I& other)
			{
				Color3I result = *this * other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const Color3I& other)
			{
				Color3I result = *this + other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Color3I& other)
			{
				Color3I result = *this - other;
				for (uint32_t i = 0; i < 3; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Color3I& other) const 
			{ 
				for (uint32_t i = 0; i < 3; i++) 
				{
					if (values[i] != other.values[i]) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Color3I& other) const { return !(*this == other); }

		};

		struct Color4I
		{
			union {
				struct {
					uint8_t r;
					uint8_t g;
					uint8_t b;
					uint8_t a;
				};
				uint8_t values[4];
			};

			Color4I() : r(0), g(0), b(0), a(0) {}  
			Color4I(const uint8_t& r, const uint8_t& g, const uint8_t& b, const uint8_t& a) : r(r), g(g), b(b), a(a) {}  

			Color4I(const Color4I& other) 
			{
				for (uint32_t i = 0; i < 4; i++)
				{
					values[i] = other.values[i];
				}
			} 

			Color4I operator / (const Color4I& other) const { return Color4I(r / other.r, g / other.g, b / b, a / a); }  
			Color4I operator * (const Color4I& other) const { return Color4I(r * other.r, g * other.g, b * b, a * a); }  
			Color4I operator + (const Color4I& other) const { return Color4I(r + other.r, g + other.g, b + b, a + a); }  
			Color4I operator - (const Color4I& other) const { return Color4I(r - other.r, g - other.g, b - b, a - a); }

			void operator /= (const Color4I& other)
			{
				Color4I result = *this / other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator *= (const Color4I& other)
			{
				Color4I result = *this * other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}
			
			void operator += (const Color4I& other)
			{
				Color4I result = *this + other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			void operator -= (const Color4I& other)
			{
				Color4I result = *this - other;
				for (uint32_t i = 0; i < 4; i++)
				{
					this->values[i] = result.values[i];
				}
			}

			bool operator == (const Color4I& other) const 
			{ 
				for (uint32_t i = 0; i < 4; i++) 
				{
					if (values[i] != other.values[i]) 
					{
						return false;
					}
    			}
				return true;
			}

			bool operator != (const Color4I& other) const { return !(*this == other); }
		};

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

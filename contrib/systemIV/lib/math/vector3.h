#ifndef VECTOR3_H
#define VECTOR3_H

#include <math.h>

#include "lib/math/vector2.h"
#include "lib/math/math_utils.h"
#include "lib/math/fixed.h"


template <class T> class Vector3
{
private:
	bool Compare(Vector3 const &b) const;

public:
	T x, y, z;
	
	// *** Class methods ***
	static Vector3 const UpVector()
	{
		return Vector3(0, 1, 0);
	}
	
	static Vector3 const ZeroVector()
	{
		return Vector3(0, 0, 0);
	}

	// *** Constructors ***
	Vector3()
	:	x(0), y(0), z(0) 
	{
	}

	inline Vector3(T _x, T _y, T _z)
	:	x(_x), y(_y), z(_z) 
	{
	}

	Vector3(Vector2 const &_b)
	:	x(_b.x), y(0), z(_b.y)
	{
	}
	
	template <class T2>
	Vector3(Vector3<T2> const &_v);

	void Zero()
	{
		x = y = z = 0;
	}

	void Set(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}
	
	Vector3 operator ^ (Vector3<T> const &b) const
	{
		return Vector3(y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x); 
	}

	T operator * (Vector3<T> const &b) const
	{
		return x*b.x + y*b.y + z*b.z;
	}

	Vector3<T> operator - () const
	{
		return Vector3(-x, -y, -z);
	}

	Vector3<T> operator + (Vector3<T> const &b) const
	{
		return Vector3(x + b.x, y + b.y, z + b.z);
	}

	Vector3<T> operator - (Vector3<T> const &b) const
	{
		return Vector3(x - b.x, y - b.y, z - b.z);
	}

	Vector3<T> operator * (T const b) const
	{
		return Vector3(x * b, y * b, z * b);
	}

	Vector3<T> operator / (T const b) const
	{
		T multiplier = 1 / b;
		return Vector3(x * multiplier, y * multiplier, z * multiplier);
	}

	Vector3<T> const &operator *= (T const b)
	{
		x *= b;
		y *= b;
		z *= b;
		return *this;
	}

	Vector3<T> const &operator /= (T const b)
	{
		T multiplier = 1 / b;
		x *= multiplier;
		y *= multiplier;
		z *= multiplier;
		return *this;
	}

	Vector3<T> const &operator += (Vector3<T> const &b)
	{
		x += b.x;
		y += b.y;
		z += b.z;
		return *this;
	}

	Vector3<T> const &operator -= (Vector3<T> const &b)
	{
		x -= b.x;
		y -= b.y;
		z -= b.z;
		return *this;
	}

	Vector3<T> const &Normalise()
	{
		T lenSqrd = x*x + y*y + z*z;
		if (lenSqrd > 0)
		{
			T invLen = 1 / sqrt(lenSqrd);
			x *= invLen;
			y *= invLen;
			z *= invLen;
		}
		else
		{
			x = y = 0;
			z = 1;
		}

		return *this;
	}

	Vector3 const &SetLength(T _len)
	{
		T mag = Mag();
		if (NearlyEquals(mag, 0))
		{	
			x = _len;
			return *this;
		}

		T scaler = _len / Mag();
		*this *= scaler;
		return *this;
	}


	bool operator == (Vector3 const &b) const // Uses FLT_EPSILON for floats
	{
		return Compare(b);
	}
	
	bool operator != (Vector3 const &b) const // Uses FLT_EPSILON for floats
	{
		return !Compare(b);
	}

	void RotateAroundX (T _angle)
	{
		FastRotateAround(Vector3(1,0,0), _angle);
	}

	void RotateAroundY (T _angle)
	{
		FastRotateAround(Vector3(0, 1, 0), _angle);
	}
	
	void RotateAroundZ (T _angle)
	{
		FastRotateAround(Vector3(0,0,1), _angle);
	}
	
	// ASSUMES that _norm is normalised
	void FastRotateAround(Vector3 const &_norm, T _angle)
	{
		T dot = (*this) * _norm;
		Vector3 a = _norm * dot;
		Vector3 n1 = *this - a;
		Vector3 n2 = _norm ^ n1;
		T s = sin( _angle );
		T c = cos( _angle );

		*this = a + T(c)*n1 + T(s)*n2;
	}
	
	void RotateAround (Vector3 const &_axis)
	{
		T angle = _axis.MagSquared();
		if (angle < 1e-8) return;
		angle = sqrtf(angle);
		Vector3 norm(_axis / angle);
		FastRotateAround(norm, angle);
	}
	
	T Mag() const
	{
        T magSqd = MagSquared();     
        return sqrt(magSqd);		
	}

	T MagSquared() const
	{
		return x * x + y * y + z * z;
	}

	void HorizontalAndNormalise()
	{
		y = 0;
		T invLength = 1 / sqrtf(x * x + z * z);
		x *= invLength;
		z *= invLength;
	}

	float *GetData()
	{
		return &x;
	}

	float const *GetDataConst() const
	{
		return &x;
	}
};

// Operator * between float and Vector3
template <class T>
inline Vector3<T> operator * (	T _scale, Vector3<T> const &_v )
{
	return _v * _scale;
}

#endif

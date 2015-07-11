#include "lib/universal_include.h"

#include <math.h>
#include <float.h>

#include "vector2.h"
#include "vector3.h"
#include "math_utils.h"
#include "lib/math/fixed.h"

template <>
bool Vector3<float>::Compare(Vector3<float> const &b) const
{
	return ( NearlyEquals(x, b.x) &&
			 NearlyEquals(y, b.y) &&
			 NearlyEquals(z, b.z) );
}

template <>
bool Vector3<Fixed>::Compare(Vector3<Fixed> const &b) const
{
	return ( x == b.x && y == b.y && z == b.z );
}

template <> template <>
Vector3<float>::Vector3(Vector3<Fixed> const &_v)
{
	x = _v.x.DoubleValue();
	y = _v.y.DoubleValue();
	z = _v.z.DoubleValue();
}

template <> template <>
Vector3<Fixed>::Vector3(Vector3<float> const &_v)
: x(Fixed::FromDouble(_v.x)), y(Fixed::FromDouble(_v.y)), z(Fixed::FromDouble(_v.z))
{
}
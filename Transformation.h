#pragma once
#include "cyVector.h"

class Transformation
{
public:
	cyVec3f rotation;
	cyVec3f translation;
	cyVec3f scale;
	float perspective_degrees = 45.0f;

	Transformation() : rotation(cyVec3f(0.0, 0.0, 0.0)), translation(cyVec3f(0.0, 0.0, 0.0)), scale(cyVec3f(1.0, 1.0, 1.0)) {}
	Transformation(cyVec3f rotation, cyVec3f translation, cyVec3f scale) : rotation(rotation), translation(translation), scale(scale) {}
	Transformation(cyVec3f rotation) : rotation(rotation), translation(cyVec3f(0.0, 0.0, 0.0)), scale(cyVec3f(1.0f, 1.0f, 1.0f )) {}

	~Transformation() {}

	void IncrementRotation(float x, float y, float z) { rotation.Set(rotation.x + x, rotation.y + y, rotation.z + z); }
	void IncrementTranslation(float x, float y, float z) { translation.Set(translation.x + x, translation.y + y, translation.z + z); }
	void SetScale(cyVec3f new_scale) { scale = new_scale; }
	void SetScale(float new_scale) { scale = cyVec3f(new_scale, new_scale, new_scale); }
	float GetUniformScale() { return scale.x; }
};


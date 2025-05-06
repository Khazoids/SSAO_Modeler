#ifndef SCENE_H
#define SCENE_H
#include "cyVector.h"
#include "Transformation.h"
#include <vector>
using namespace std;

class Scene {
	public:
		Transformation& scene;
		
		Scene(Transformation& scene) : scene(scene){}


		~Scene() {}

		void IncrementRotation(float x, float y, float z) 
		{ 
			for (Transformation* transformation : transformations)
			{
				transformation->IncrementRotation(x, y, z);
			}
		}

		void IncrementTranslation(float x, float y, float z) 
		{
			for (Transformation* transformation : transformations)
			{
				transformation->IncrementTranslation(x, y, z);
			}
		}

		void SetScale(cyVec3f new_scale)
		{ 
			for (Transformation* transformation : transformations)
			{
				transformation->SetScale(new_scale);
			}
		}

		void SetScale(float new_scale) 
		{ 
			for (Transformation* transformation : transformations)
			{
				transformation->SetScale(new_scale);
			}
		}
		
		void AddTransformation(Transformation* t)
		{
			transformations.push_back(t);
		}
	private:
		vector<Transformation *> transformations;
};
#endif
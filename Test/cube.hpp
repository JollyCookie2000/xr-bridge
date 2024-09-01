// Author: Lorenzo Adam Piazza

#pragma once

#include <glm/glm.hpp>

class Cube
{
public:
	Cube();
	~Cube();

	void render(const glm::mat4 matrix) const;
private:
	unsigned int shader;
	unsigned int vao;
	unsigned int vbo;
};

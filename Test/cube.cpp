// Author: Lorenzo Adam Piazza

#include "cube.hpp"

#include <string>
#include <vector>

#include <GL/glew.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

Cube::Cube() :
	shader { 0 },
	vao { 0 },
	vbo { 0 }
{
	const std::string vertex_shader_source = R"(
		#version 440 core

		uniform mat4 matrix;

		layout(location = 0) in vec3 position;

		out vec3 pos;

		void main(void)
		{
			gl_Position = matrix * vec4(position, 1.0f);
			pos = position;
		}
	)";

	const std::string fragment_shader_source = R"(
		#version 440 core

		in vec3 pos;

		out vec4 fragment;

		void main(void)
		{
			//fragment = vec4(0.46f, 0.27f, 0.27f, 1.0f);
			fragment = vec4(pos, 1.0f);
		}
	)";

	const unsigned int vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
	const char* vertex_source = vertex_shader_source.c_str();
	glShaderSource(vertex_shader_id, 1, (const char**) &vertex_source, nullptr);
	glCompileShader(vertex_shader_id);

	GLint success;
	glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		throw "Failed to compile vertex shader.";
	}

	const unsigned int fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
	const char* fragment_shader = fragment_shader_source.c_str();
	glShaderSource(fragment_shader_id, 1, (const char**) &fragment_shader, nullptr);
	glCompileShader(fragment_shader_id);

	glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		throw "Failed to compile fragment shader.";
	}

	this->shader = glCreateProgram();
	glAttachShader(this->shader, fragment_shader_id);
	glAttachShader(this->shader, vertex_shader_id);
	glLinkProgram(this->shader);
	glDeleteShader(vertex_shader_id);
	glDeleteShader(fragment_shader_id);

	////////////////////////////////////////////////////////////////////////////

	const std::vector<float> vertices = {
		-1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f, 1.0f,-1.0f,
		1.0f,-1.0f, 1.0f,
		-1.0f,-1.0f,-1.0f,
		1.0f,-1.0f,-1.0f,
		1.0f, 1.0f,-1.0f,
		1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f,-1.0f,
		1.0f,-1.0f, 1.0f,
		-1.0f,-1.0f, 1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f,-1.0f, 1.0f,
		1.0f,-1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f,-1.0f,-1.0f,
		1.0f, 1.0f,-1.0f,
		1.0f,-1.0f,-1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f,-1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f,-1.0f,
		-1.0f, 1.0f,-1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f,-1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f,-1.0f, 1.0f
	};

	glGenVertexArrays(1, &this->vao);
	glBindVertexArray(this->vao);

	glGenBuffers(1, &this->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

Cube::~Cube()
{
	glDeleteProgram(this->shader);
	glDeleteVertexArrays(1, &this->vao);
	glDeleteBuffers(1, &this->vbo);
}

void Cube::render(const glm::mat4 matrix) const
{
	const int matrix_uniform_location = glGetUniformLocation(this->shader, "matrix");
	glUniformMatrix4fv(matrix_uniform_location, 1, GL_FALSE, glm::value_ptr(matrix));

	glUseProgram(this->shader);

	glBindVertexArray(this->vao);
	glDrawArrays(GL_TRIANGLES, 0, 12 * 3);

	glBindVertexArray(0);
}

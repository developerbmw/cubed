#include "rendering_engine.h"
#include "uniform.h"
#include <utility>

void Uniform::update(RenderingEngine& re)
{
	switch(m_type)
	{
		case UNIFORMTYPE_VEC4:
			set_value(re.get_vec4(m_name));
			break;

		case UNIFORMTYPE_MAT4:
			set_value(re.get_mat4(m_name));
			break;
	}
}

void Uniform::set_value(const glm::vec4& value)
{
	glUniform4fv(m_location, 1, &value[0]);
}

void Uniform::set_value(const glm::mat4& value)
{
	glUniformMatrix4fv(m_location, 1, GL_FALSE, &value[0][0]);
}
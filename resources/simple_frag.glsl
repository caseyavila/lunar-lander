#version 330 core 

out vec4 color;

uniform vec3 MatAmb;
uniform vec3 MatSpec;
uniform float MatShine;

//interpolated normal and light vector in camera space
in vec3 fragNor;
in vec3 lightDir;
//position of the vertex in camera space
in vec3 EPos;

void main()
{
	//you will need to work with these for lighting
	vec3 normal = normalize(fragNor);
	vec3 light = normalize(lightDir);

    float d_c = dot(normal, light);

    vec3 reflectDir = normalize(reflect(-light, normal));

    vec3 spec = pow(max(dot(normalize(-EPos), reflectDir), 0.0), MatShine) * MatSpec;

	color = vec4(MatAmb * 10.0 * d_c + spec, 1.0);
}

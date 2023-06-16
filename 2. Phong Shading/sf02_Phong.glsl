
#version 400

// Phong reflection model
uniform vec3	LightPosition0;					// In the view coordinate system
uniform vec3	LightPosition1;					// In the view coordinate system
uniform vec3	Kd = vec3(0.95, 0.95, 0.95);	// Diffuse reflectivity
uniform vec3	Ka = vec3(0.10, 0.10, 0.10);	// Ambient reflectivity
uniform vec3	Ks = vec3(0.75, 0.75, 0.75);	// Specular reflectivity
uniform float	Shininess = 20.0;				// Specular shininess factor

struct Light
{
	vec3	position;		// eye-space
	vec3	ambient;
	vec3	diffuse;
	vec3	specular;
	vec3	attenuation;	// constant, linear, quadratic;
};

Light L0 = Light(
	LightPosition0,			// Position in the eye space
	vec3(1.0, 1.0, 1.0),	// Ambient 
	vec3(1.0, 1.0, 1.0),	// Diffuse
	vec3(1.0, 1.0, 1.0),	// Specular
	vec3(1.0, 0.0, 0.0)		// Attenuation (const, linear, quad)
);

Light L1 = Light(
	LightPosition1,			// Position in the eye space
	vec3(0.1, 0.1, 0.1),	// Ambient 
	vec3(0.2, 0.2, 0.2),	// Diffuse
	vec3(0.8, 0.8, 0.8),	// Specular
	vec3(1.0, 0.0, 0.0)		// Attenuation (const, linear, quad)
);


vec3
phongReflectionModel(Light L0, vec3 P, vec3 N)
{
	// Calculate the ambient term
	vec3 color = Ka * L0.ambient;

	// Direction and distance to the light
	vec3	L = L0.position - P;
	float	l = length(L);
			L = normalize(L);

	// Light attenuation
	float	attenuation = L0.attenuation.x + L0.attenuation.y * l + L0.attenuation.z * l*l;
	attenuation = clamp(1.0/attenuation, 0, 1);

	// Lambertian for the diffuse term
	float lambertian = max(dot(N, L), 0.0);

	// Diffuse term
	color += (attenuation *  lambertian) *  Kd * L0.diffuse;

	// Specular term
	if (lambertian > 0)
	{
		vec3	R = normalize(-reflect(L, N));
		vec3	V = normalize(-P);	// We are in Eye Coordinates, so EyePos is (0, 0, 0).

		float	specular = pow(max(dot(R, V), 0.0), Shininess);
		color +=  (attenuation * specular) * Ks * L0.specular;
	}

	return color;
}

in vec3	position;
in vec3 normal;

out vec3 outColor;

void
main(void)
{
	// Lighting

	vec3 color0 = phongReflectionModel(L0, position, normal);
	vec3 color1 = phongReflectionModel(L1, position, normal);

	outColor = color0+color1;

}

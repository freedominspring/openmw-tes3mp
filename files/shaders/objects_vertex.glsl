#version 120

#if @diffuseMap
varying vec2 diffuseMapUV;
#endif

varying float depth;

varying vec3 lighting;

#define MAX_LIGHTS 8

vec3 doLighting(vec4 vertex, vec3 normal, vec3 vertexColor)
{
    vec3 viewPos = (gl_ModelViewMatrix * vertex).xyz;
    vec3 viewNormal = normalize((gl_NormalMatrix * normal).xyz);
    vec3 lightDir;
    float d;

#if @colorMode == 2
    vec3 diffuse = vertexColor;
    vec3 ambient = vertexColor;
#else
    vec3 diffuse = gl_FrontMaterial.diffuse.xyz;
    vec3 ambient = gl_FrontMaterial.ambient.xyz;
#endif

    vec3 lightResult = vec3(0.0, 0.0, 0.0);
    for (int i=0; i<MAX_LIGHTS; ++i)
    {
        lightDir = gl_LightSource[i].position.xyz - (viewPos * gl_LightSource[i].position.w);
        d = length(lightDir);
        lightDir = normalize(lightDir);

        lightResult.xyz += ambient * gl_LightSource[i].ambient.xyz;
        lightResult.xyz += diffuse * gl_LightSource[i].diffuse.xyz * clamp(1.0 / (gl_LightSource[i].constantAttenuation + gl_LightSource[i].linearAttenuation * d + gl_LightSource[i].quadraticAttenuation * d * d), 0.0, 1.0)
                * max(dot(viewNormal.xyz, lightDir), 0.0);
    }

    lightResult += gl_LightModel.ambient.xyz * ambient;

    return lightResult;
}


void main(void)
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    depth = gl_Position.z;

#if @diffuseMap
    diffuseMapUV = gl_MultiTexCoord@diffuseMapUV.xy;
#endif

    lighting = doLighting(gl_Vertex, gl_Normal.xyz, gl_Color.xyz);
    lighting = clamp(lighting, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
}

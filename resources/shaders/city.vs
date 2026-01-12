#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input instance attributes (Raylib maps these automatically)
in mat4 instanceTransform;

// Input uniforms
uniform mat4 mvp;
uniform mat4 view;
uniform mat4 projection;

// Output to Fragment Shader
out vec2 fragTexCoord;
out vec3 fragNormal;
out float fragDist;

void main()
{
    // 1. Calculate World Position using the INSTANCE matrix
    vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);
    
    // 2. Calculate View Position (Camera Space) for Fog
    vec4 viewPos = view * worldPos;
    fragDist = length(viewPos.xyz);
    
    // 3. Pass Data
    fragTexCoord = vertexTexCoord;
    // Rotate normal by instance rotation (upper 3x3)
    fragNormal = normalize(mat3(instanceTransform) * vertexNormal);
    
    // 4. Final Clip Position
    gl_Position = projection * viewPos;
}
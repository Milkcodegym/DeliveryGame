#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in float fragDist;

out vec4 finalColor;

// Uniforms
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom Uniforms
uniform vec3 lightDir;     // Direction of the moon/streetlights
uniform vec4 fogColor;     // Color of the background
uniform float fogStart;    // Where fog begins
uniform float fogEnd;      // Where geometry is fully invisible
uniform float emissive;    // 0.0 for buildings, 1.0 for windows (glowing)

void main()
{
    // 1. Base Texture Color
    vec4 texColor = texture(texture0, fragTexCoord) * colDiffuse;
    
    // 2. Simple Lighting (Lambertian)
    // If object is emissive (windows), ignore lighting shadows
    float NdotL = max(dot(fragNormal, normalize(lightDir)), 0.0);
    vec3 ambient = vec3(0.3); // Base darkness
    vec3 diffuse = vec3(0.7) * NdotL;
    vec3 lighting = mix(ambient + diffuse, vec3(1.0), emissive);
    
    vec3 litColor = texColor.rgb * lighting;

    // 3. Linear Fog Calculation
    // Calculate factor: 0.0 = Clear, 1.0 = Fully Fogged
    float fogFactor = clamp((fragDist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    
    // 4. Mix Geometry with Fog Color
    vec3 finalRGB = mix(litColor, fogColor.rgb, fogFactor);
    
    finalColor = vec4(finalRGB, texColor.a);
}
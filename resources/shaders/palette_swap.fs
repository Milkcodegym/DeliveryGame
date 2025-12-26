#version 330

// Input from Vertex Shader
in vec2 fragTexCoord;
in vec4 fragColor;

// Output to Screen
out vec4 finalColor;

// Uniforms (Variables we send from C)
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// The color to replace (Beige: 239, 213, 185) -> Normalized: (0.93, 0.83, 0.72)
const vec3 targetColor = vec3(0.937, 0.835, 0.725); 
const float threshold = 0.15; // How strict is the color match?

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Calculate distance between current pixel and Beige
    float dist = distance(texelColor.rgb, targetColor);
    
    vec4 resultColor = texelColor;
    
    // If pixel is close to Beige, tint it with the building's assigned color (fragColor)
    if (dist < threshold)
    {
        // Multiply the grayscale value of the texture by the input color
        // We preserve the texture's shading (light/dark) but change the hue
        float luminance = dot(texelColor.rgb, vec3(0.299, 0.587, 0.114));
        resultColor = vec4(fragColor.rgb * luminance * 1.5, texelColor.a);
    }
    
    finalColor = resultColor * colDiffuse;
}
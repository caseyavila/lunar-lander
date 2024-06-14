#version 330 core
uniform sampler2D Texture0;
uniform sampler2D skytex;
uniform bool flip;

in vec2 vTexCoord;
out vec4 Outcolor;
in vec3 lightDir;
in vec3 fragNor;

void main() {
  vec4 texColor0 = texture(Texture0, vTexCoord);
  vec4 texColorsky = texture(skytex, normalize(fragNor).yz);

  	//to set the out color as the texture color 
    if (flip) {
      Outcolor = texColor0;
    } else {
      float d_c = dot(normalize(fragNor), normalize(lightDir));
      //Outcolor = d_c * texColor0;
      //Outcolor = d_c * texColorsky;
      Outcolor = d_c * (1.5 * texColorsky + 0.5 * texColor0);
    }
}


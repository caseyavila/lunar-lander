#version 330 core
uniform sampler2D Texture0;
uniform bool flip;

in vec2 vTexCoord;
out vec4 Outcolor;
in vec3 lightDir;
in vec3 fragNor;

void main() {
  vec4 texColor0 = texture(Texture0, vTexCoord);

  	//to set the out color as the texture color 
    if (flip) {
      Outcolor = texColor0;
    } else {
      float d_c = dot(normalize(fragNor), normalize(lightDir));
      Outcolor = d_c * texColor0;
    }
  
  	//to set the outcolor as the texture coordinate (for debugging)
	  //Outcolor = vec4(vTexCoord.s, vTexCoord.t, 0, 1);
}


#version 300 es
precision mediump float;

// Flattened fog uniforms (WebGL1 has no UBOs)
uniform vec4        vFogColor;

uniform sampler2D   shaderTexture;

// Varyings from vertex shader
in vec4     _psin_color;
in vec2     _psin_tex;
in float    _psin_fogfactor;

out vec4    _psout_color;

void main()
{
  vec4 textureColor = texture( shaderTexture, _psin_tex ) * _psin_color;
  textureColor.rgb = mix( textureColor.rgb, vFogColor.rgb, _psin_fogfactor );

  _psout_color = textureColor;
}

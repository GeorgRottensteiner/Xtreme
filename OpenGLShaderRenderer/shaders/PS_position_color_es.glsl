#version 300 es
precision mediump float;

uniform vec4  vFogColor;

in vec4     _psin_color;
in float    _psin_fogfactor;

out vec4    _psout_color;



void main()
{
  _psout_color = mix( _psin_color, vec4( vFogColor.rgb, 1.0 ), _psin_fogfactor );
}

noperspective in vec2 VertexUV;

uniform mat4x2 gPos;
uniform vec4 gColor;
uniform vec2 gRectSize;
uniform vec4 gRadii;

out vec4 FragClr;

void main()
{
    vec2 p = (VertexUV - 0.5) * gRectSize;

    vec2 selectRadius = (p.x > 0.0) ? gRadii.xy : gRadii.zw;
    float r = (p.y > 0.0) ? selectRadius.x : selectRadius.y;
    r = min(r, min(gRectSize.x, gRectSize.y) * 0.5);

    vec2 q = abs(p) - gRectSize * 0.5 + r;
    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;

    float alpha = 1.0 - smoothstep(0.0, fwidth(dist), dist);
    FragClr = vec4(gColor.rgb, gColor.a * alpha);
}

uniform mat4x2 gPos;
uniform vec2 gRectPos;
uniform vec2 gRectSize;

noperspective out vec2 VertexUV;

void main()
{
    VertexUV = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
    gl_Position = vec4(gPos * vec4(gRectPos + gRectSize * VertexUV, 0.0, 1.0), 0.0, 1.0);
}

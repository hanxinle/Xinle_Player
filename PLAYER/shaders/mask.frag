#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
// 从外部传入多边形顶点坐标，每两个 float 组成一个 vec2(x, y)。
uniform float pointData[100];
uniform int numPoints;
uniform vec2 resolution;
uniform float mask_blur;
uniform bool isHasChromakey;

bool isPointInPolygon(vec2 p, vec2[100] vertices, int numVertices) {
    bool isInside = false;
    for (int i = 0, j = numVertices - 1; i < numVertices; j = i++) {
        if ((vertices[i].y > p.y) != (vertices[j].y > p.y) &&
            (p.x < (vertices[j].x - vertices[i].x) * (p.y - vertices[i].y) /
                       (vertices[j].y - vertices[i].y) + vertices[i].x)) {
            isInside = !isInside;
        }
    }
    return isInside;
}

float distanceToEdge(vec2 p, vec2 v0, vec2 v1) {
    vec2 e = v1 - v0;
    vec2 pv0 = p - v0;
    float t = dot(pv0, e) / dot(e, e);
    t = clamp(t, 0.0, 1.0);
    vec2 projection = v0 + t * e;
    return length(p - projection);
}

float getMinDistance(vec2 p, vec2[100] vertices, int numVertices) {
    float minDistance = distanceToEdge(p, vertices[0], vertices[1]);
    for (int i = 1; i < numVertices - 1; ++i) {
        float d = distanceToEdge(p, vertices[i], vertices[i + 1]);
        minDistance = min(minDistance, d);
    }
    float d = distanceToEdge(p, vertices[numVertices - 1], vertices[0]);
    minDistance = min(minDistance, d);
    return minDistance;
}

void main() {
    vec2 pointCoordinates[100];
    for (int i = 0; i < numPoints; ++i) {
        pointCoordinates[i] = vec2(pointData[2 * i], pointData[2 * i + 1]);
    }

    vec2 polygonVertices[100];
    for (int i = 0; i < numPoints; ++i) {
        polygonVertices[i] = pointCoordinates[i];
    }

    vec2 currentPixel = gl_FragCoord.xy / resolution;
    // gl_FragCoord.y 是左下角原点，这里翻转为左上角原点，与传入多边形坐标一致。
    currentPixel.y = 1.0 - currentPixel.y;
    bool insidePolygon = isPointInPolygon(currentPixel, polygonVertices, numPoints);

    float distanceToEdgeValue = getMinDistance(currentPixel, polygonVertices, numPoints);

    vec4 fillColor = texture(image, vTexCoord);
    float m = 0.001;
    float f = mask_blur / 2000.0;

    if (insidePolygon) {
        m = smoothstep(m - f, m + f, distanceToEdgeValue - f);
        FragColor = fillColor;
    } else {
        float alpha = smoothstep(m - f, m + f, distanceToEdgeValue - f);
        m = alpha;
        if (isHasChromakey) {
            FragColor = vec4(fillColor.r, fillColor.g, fillColor.b, 1.0 - m);
        } else {
            FragColor = vec4(fillColor.r * (1.0 - m), fillColor.g * (1.0 - m),
                             fillColor.b * (1.0 - m), 1.0 - m);
        }
    }
}

#version 300 es
precision highp float;

// Output color for the fragment
out vec4 FragColor;

// Uniforms to control the scene and camera
uniform vec2 u_resolution; // The resolution of the viewport (width, height)
uniform float u_time;      // Time in seconds, for animations
uniform vec3 u_cameraPos;  // The position of the camera in world space
uniform vec3 u_cameraLookAt; // The point the camera is looking at

// --- Constants and Structs ---

const float MAX_DIST = 100.0; // Maximum distance to raymarch
const float SURF_DIST = 0.001; // Epsilon for surface intersection
const int MAX_STEPS = 100;    // Maximum number of raymarching steps

// A struct to hold information about a ray-scene intersection
struct HitInfo {
    float dist;     // Distance from the ray origin
    vec3 pos;       // Position of the hit in world space
    int materialID; // An ID to identify which object was hit
};

// --- Signed Distance Functions (SDFs) ---
// These functions return the shortest distance from a point 'p' to a surface.
// A negative return value means 'p' is inside the object.

// SDF for a plane at y=0
float sdPlane(vec3 p) {
    return p.y;
}

// SDF for a sphere
float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

// SDF for a box
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// --- Scene Definition ---
// This function combines all SDFs in the world to find the closest surface at point 'p'.
// It returns a vec2 containing:
//  x: The shortest distance to any object in the scene.
//  y: The material ID of that closest object.
vec2 mapTheWorld(vec3 p) {
    vec2 res = vec2(p.y, 0); // Start with the ground plane (material 0)

    // Add a sphere that moves over time
    vec3 spherePos = vec3(sin(u_time) * 2.0, 1.0, cos(u_time) * 2.0);
    float sphereDist = sdSphere(p - spherePos, 1.0);
    if (sphereDist < res.x) {
        res = vec2(sphereDist, 1); // Material 1 for the sphere
    }

    // Add a static box
    vec3 boxPos = vec3(-2.5, 0.5, 0.0);
    float boxDist = sdBox(p - boxPos, vec3(0.5));
    if (boxDist < res.x) {
        res = vec2(boxDist, 2); // Material 2 for the box
    }

    return res;
}

// --- Core Raymarching Logic ---

// The main raymarching algorithm
HitInfo raymarch(vec3 ro, vec3 rd) {
    float dO = 0.0; // Distance from Origin
    int matID = -1;

    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * dO;
        vec2 dS = mapTheWorld(p);
        float distToScene = dS.x;
        matID = int(dS.y);

        if (distToScene < SURF_DIST) {
            // We hit something!
            return HitInfo(dO, p, matID);
        }
        if (dO > MAX_DIST) {
            // We've gone too far
            break;
        }
        dO += distToScene;
    }
    return HitInfo(MAX_DIST, ro + rd * MAX_DIST, -1); // Return a miss
}

// Calculate the surface normal at a point 'p' using the gradient of the SDF
vec3 calcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    // The gradient is the vector of partial derivatives, approximated by finite differences
    return normalize(vec3(
                     mapTheWorld(p + e.xyy).x - mapTheWorld(p - e.xyy).x,
                     mapTheWorld(p + e.yxy).x - mapTheWorld(p - e.yxy).x,
                     mapTheWorld(p + e.yyx).x - mapTheWorld(p - e.yyx).x
                     ));
}

// --- Grid and Lighting ---

// THE CORE GRID LOGIC
// Calculates a grid pattern for a given world-space position 'p'.
// Returns a blend factor: 1.0 for no line, < 1.0 for on a line.
float getGrid(vec3 p) {
    // Define grid line colors and spacing
    float majorSpacing = 5.0;
    float minorSpacing = 1.0;

    // Use the XZ plane for the grid coordinates
    vec2 coord = p.xz;

    // --- Anti-Aliased Grid Lines using fwidth() ---
    // fwidth() gives the sum of the absolute derivatives in x and y.
    // It tells us how much 'coord' changes between this pixel and the next.
    // This allows us to draw lines that are always 1 pixel thick, regardless of distance or angle.
    vec2 derivative = fwidth(coord);

    // Minor grid lines
    vec2 grid = abs(fract(coord / minorSpacing - 0.5) - 0.5) / derivative;
    float minorLine = min(grid.x, grid.y);

    // Major grid lines (thicker)
    grid = abs(fract(coord / majorSpacing - 0.5) - 0.5) / derivative;
    float majorLine = min(grid.x, grid.y);

    // Combine the lines. We want minor lines to be subtle and major lines to be prominent.
    // A value of 1.0 means no line. A value close to 0.0 means a line is drawn.
    float gridFactor = min(
        smoothstep(0.0, 1.0, minorLine), // Soft minor lines
        smoothstep(0.0, 0.3, majorLine)  // Sharp major lines
    );

    // Fade the grid out in the distance to reduce noise
    float fade = 1.0 - exp(-p.y * 0.1);

    return mix(gridFactor, 1.0, 1.0 - fade);
}


// Simple Blinn-Phong lighting model
vec3 getLight(vec3 p, vec3 normal, vec3 ro, vec3 diffuseColor) {
    vec3 lightPos = vec3(5.0, 5.0, -5.0);
    vec3 lightDir = normalize(lightPos - p);
    vec3 viewDir = normalize(ro - p);

    // Ambient light
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * vec3(1.0);

    // Diffuse light
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Specular light
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * vec3(1.0);

    // Shadow (simple hard shadow)
    HitInfo shadowHit = raymarch(p + normal * SURF_DIST * 2.0, lightDir);
    float shadow = (shadowHit.dist < length(lightPos - p)) ? 0.3 : 1.0;

    return (ambient + (diffuse + specular) * shadow) * diffuseColor;
}


// --- Main Rendering Function ---

void main() {
    // --- Camera Setup ---
    // Create a view matrix to orient the scene
    vec3 forward = normalize(u_cameraLookAt - u_cameraPos);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);

    // Calculate the ray direction for the current fragment
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
    vec3 rd = normalize(uv.x * right + uv.y * up + 1.5 * forward); // 1.5 is FOV

    // --- Raymarch and Render ---
    HitInfo hit = raymarch(u_cameraPos, rd);

    vec3 finalColor = vec3(0.0);

    if (hit.materialID == -1) {
        // No object was hit, draw a sky gradient
        finalColor = mix(vec3(0.5, 0.7, 0.9), vec3(0.2, 0.3, 0.5), rd.y);
    } else {
        vec3 normal = calcNormal(hit.pos);
        vec3 baseColor;

        // Determine base color based on which object was hit
        if (hit.materialID == 0) { // Ground Plane
                                   baseColor = vec3(0.7, 0.7, 0.7);

                                   // --- APPLY THE GRID ---
                                   // Calculate the grid factor and mix it with the base color.
                                   float gridFactor = getGrid(hit.pos);
                                   vec3 gridColor = vec3(0.2); // Dark gray grid lines
                                   baseColor = mix(gridColor, baseColor, gridFactor);

        } else if (hit.materialID == 1) { // Sphere
                                          baseColor = vec3(0.9, 0.3, 0.2); // Red
        } else { // Box
                 baseColor = vec3(0.2, 0.4, 0.9); // Blue
        }

        // Apply lighting to the final base color
        finalColor = getLight(hit.pos, normal, u_cameraPos, baseColor);
    }

    // Gamma correction and output
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    FragColor = vec4(finalColor, 1.0);
}

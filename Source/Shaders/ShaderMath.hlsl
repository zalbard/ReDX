static const float M_E        = 2.71828175f;   // e
static const float M_LOG2E    = 1.44269502f;   // log2(e)
static const float M_LOG10E   = 0.434294492f;  // log10(e)
static const float M_LN2      = 0.693147182f;  // ln(2)
static const float M_LN10     = 2.30258512f;   // ln(10)
static const float M_PI       = 3.14159274f;   // pi
static const float M_PI_2     = 1.57079637f;   // pi/2
static const float M_PI_4     = 0.785398185f;  // pi/4
static const float M_1_PI     = 0.318309873f;  // 1/pi
static const float M_2_PI     = 0.636619747f;  // 2/pi
static const float M_2_SQRTPI = 1.12837923f;   // 2/sqrt(pi)
static const float M_SQRT2    = 1.41421354f;   // sqrt(2)
static const float M_SQRT1_2  = 0.707106769f;  // 1/sqrt(2)

// Computes the square of the value.
float sq(float v) {
    return v * v;
}

// Returns 1 for non-negative components, -1 otherwise.
// Ignores the sign of negative zero.
float2 nonNegative(const float2 p) {
    return float2((p.x >= 0.f) ? 1.f : -1.f,
                  (p.y >= 0.f) ? 1.f : -1.f);
}

// Performs Octahedral Normal Vector encoding.
// Input:  normalized 3D vector 'n' on a sphere.
// Output: 2D point 'p' on a square [-1, 1] x [-1, 1].
float2 encodeOctahedral(const float3 n) {
    // Project the sphere onto the octahedron, and then onto the XY plane.
    const float2 p = n.xy * (1.f / (abs(n.x) + abs(n.y) + abs(n.z)));
    // Reflect the folds of the lower hemisphere over the diagonals.
    return (n.z <= 0.f) ? ((1.f - abs(p.yx)) * nonNegative(p)) : p;
}

// Performs Octahedral Normal Vector decoding.
// Input:  2D point 'p' on a square [-1, 1] x [-1, 1].
// Output: normalized 3D vector 'n' on a sphere.
float3 decodeOctahedral(const float2 p) {
    float3 n = float3(p.xy, 1.f - abs(p.x) - abs(p.y));
    if (n.z < 0) {
        n.xy = (1.f - abs(n.yx)) * nonNegative(n.xy);
    }
    return normalize(n);
}

// Returns the index of the largest component of the 3D vector 'v'.
uint largestComponent(const float3 v) {
    // TODO: use V_CUBEID_F32 on AMD.
    uint id;
    if (abs(v.y) > abs(v.x)) {
        id = abs(v.z) > abs(v.y) ? 2 : 1;
    } else {
        id = abs(v.z) > abs(v.x) ? 2 : 0;
    }
    return id;
}

// Returns the index of the largest component of 4D the vector 'v'.
uint largestComponent(const float4 v) {
    uint id = largestComponent(v.xyz);
    id = abs(v.w) > abs(v[id]) ? 3 : id;
    return id;
}

// Packs the quaternion 'q' into the R10G10B10A2_UNORM format.
// See "Rendering The World Of Far Cry 4" by Stephen McAuley.
float4 packQuaternion(float4 q) {
    const uint id = largestComponent(q);
    // Swap the largest component with the W component.
    switch (id) {
        case 0: q = q.wyzx; break;
        case 1: q = q.xwzy; break;
        case 2: q = q.xywz; break;
    }
    // X, Z, Y components now lie within [-sqrt(0.5), sqrt(0.5)].
    // Negate the quaternion so that we can assume that W is non-negative.
    // The negated quaternion represents the same rotation.
    q.xyz = (q.w >= 0.f) ? q.xyz : -q.xyz;
    // Transform X, Z, Y components from [-sqrt(0.5), sqrt(0.5)] to [0, 1].
    q.xyz = q.xyz * M_SQRT1_2 + 0.5f;
    // Store the index.
    q.w   = id * rcp(3.f);
    return q;
}

// Unpacks the quaternion 'q' from the R10G10B10A2_UNORM format.
// See "Rendering The World Of Far Cry 4" by Stephen McAuley.
float4 unpackQuaternion(float4 q) {
    // Get the index of the largest component.
    const uint id = (uint)(q.w * 3.f);
    // Transform X, Z, Y components from [0, 1] to [-sqrt(0.5), sqrt(0.5)].
    q.xyz = q.xyz * M_SQRT2 - M_SQRT1_2;
    // Compute the W component.
    q.w = sqrt(1.f - saturate(dot(q.xyz, q.xyz)));
    // Swap the W component with the component at the recorded index.
    switch (id) {
        case 0: q = q.wyzx; break;
        case 1: q = q.xwzy; break;
        case 2: q = q.xywz; break;
    }
    return q;
}

// Converts the quaternion 'q' into a rotation matrix, which is interpreted s.t.
// the 1st row is the tangent, the 2nd row is the normal, the 3rd row the bitangent.
float3x3 convertQuaternionToTangentFrame(const float4 q) {
    // Tangent.
    const float3 T = float3( 1.f,  0.f,  0.f)
                   + float3(-2.f,  2.f, -2.f) * q.y * q.yxw
                   + float3(-2.f,  2.f,  2.f) * q.z * q.zwx;
    // Normal.
    const float3 N = float3( 0.f,  1.f,  0.f)
                   + float3( 2.f, -2.f,  2.f) * q.x * q.yxw
                   + float3(-2.f, -2.f,  2.f) * q.z * q.wzy;
    // Bitangent.
    const float3 B = float3( 0.f,  0.f,  1.f)
                   + float3( 2.f, -2.f, -2.f) * q.x * q.zwx
                   + float3( 2.f,  2.f, -2.f) * q.y * q.wzy;
    // Compose the rotation matrix.
    return float3x3(T, N, B);
}

// Perturbs the normalized surface normal using bump mapping technique.
// The value of the bump map at the surface position 'pos' is denoted by 'height'.
// See "Bump Mapping Unparametrized Surfaces on the GPU" by Morten Mikkelsen.
float3 perturbNormal(const float3 normal, const float3 pos, const float height) {
    // Compute the tangent frame.
    const float3 tangent   = ddx_fine(pos);
    const float3 bitangent = ddy_fine(pos);
    // Orthogonalize the tangent vectors with respect to the normal.
    const float3 vS = cross(bitangent, normal);
    const float3 vT = cross(normal, tangent);
    // Compute the determinant. Why not dot(normal, cross(tangent, bitangent))?
    const float det = dot(tangent, vS);
    // Compute partial derivatives of the height (bump) map.
    const float hS = ddx_fine(height);
    const float hT = ddy_fine(height);
    // Compute the surface gradient.
    // We avoid the division by the determinant and instead only apply its sign.
    const float  signDet  = (det >= 0.f) ? 1.f : -1.f;
    const float3 surfGrad = signDet * (hS * vS + hT * vT);
    // Compute the normal.
    // We apply scaling by the value of the determinant here.
    return normalize(normal * abs(det) - surfGrad);
}

// Evaluates the Schlick's approximation of the full Fresnel equations.
float3 evalFresnelSchlick(const float3 F0, const float3 L, const float3 H) {
    const float cosLH = saturate(dot(L, H));
    const float t  = 1.f - cosLH;
    const float t2 = sq(t);
    const float t5 = t * sq(t2);
    return F0 + (float3(1.f, 1.f, 1.f) - F0) * t5;
}

// Evaluates the GGX NDF.
float evalNdf(float roughness, const float3 N, const float3 H) {
    // Apply Disney's reparametrization.
    const float alpha = sq(roughness);
    const float cosNH = saturate(dot(N, H));
    return sq(alpha) * M_1_PI / sq(sq(cosNH) * (sq(alpha) - 1.f) + 1.f);
}

// Evaluates the combined GGX visibility term.
float evalVis(const float roughness, const float3 N, const float3 L, const float3 V) {
    const float cosNL = saturate(dot(N, L));
    const float cosNV = saturate(dot(N, V));
    // Perform Disney's hotness remapping.
    const float k = 0.125f * sq(roughness + 1.f);
    // Vis(k, N, L, V) = G2(k, N, L, V) / (4 * dot(N, L) * dot(N, V)),
    // where G2 is the Karis's approximation of the Smith's geometric term.
    return 0.25f / ((cosNL * (1.f - k) + k) * (cosNV * (1.f - k) + k));
}

// Evaluates the GGX BRDF.
float3 evalGGX(const float3 F0, const float roughness,
               const float3 N, const float3 L, const float3 V) {
    const float3 H  = normalize(L + V);
    const float3 fr = evalFresnelSchlick(F0, L, H);
    const float ndf = evalNdf(roughness, N, H);
    const float vis = evalVis(roughness, N, L, V);
    return fr * ndf * vis;
}

// Performs ACES Filmic Tone Mapping.
// See "ACES Filmic Tone Mapping Curve" by Krzysztof Narkowicz.
float3 acesFilmToneMap(const float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

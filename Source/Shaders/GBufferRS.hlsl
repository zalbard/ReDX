#define RootSig                                                                         \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "                                   \
    "SRV(t0, visibility = SHADER_VISIBILITY_PIXEL), "                                   \
    "RootConstants(num32BitConstants = 1,  b0, visibility = SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants = 12, b1, visibility = SHADER_VISIBILITY_VERTEX)"

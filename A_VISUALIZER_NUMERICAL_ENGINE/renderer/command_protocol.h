#pragma once
#include <cstdint>
#include <cstddef>

namespace bkqrgrbin {
constexpr std::uint32_t GR_BINARY_MAGIC = 0x42475142u; // 'BQGB'
constexpr std::uint32_t GR_BINARY_PROTOCOL_VERSION = 0x00016000u;

enum FrameFlags : std::uint32_t {
    FLAG_SKY = 1u << 0,
    FLAG_FIELD_DEBUG = 1u << 1,
    FLAG_CAPTURE_DEBUG = 1u << 2,
};

// Exactly 16 x 16-byte registers = 256 bytes.
struct alignas(256) BinaryFramePacket {
    std::uint32_t header0[4]; // magic, protocol, flags, renderMode
    std::uint32_t header1[4]; // width, height, maxSteps, frameId

    float camera0[4];  // camera xyz, fovY radians
    float camRight[4]; // xyz
    float camUp[4];    // xyz
    float camForward[4];// xyz

    float render0[4];  // exposure, sim time, time rate, sim dt
    float binary0[4];  // m1, m2, separation d, phase phi
    float binary1[4];  // eta, omega, d_dot, radiationReaction enabled
    float binary2[4];  // current effective object1 radius telemetry, capture u, max ray travel, sky scale OR detector half-height

    float quality0[4]; // integrator quality, baseSPP, criticalExtraSPP, pixelStride
    float quality1[4]; // maxStep, minStep, criticalGrad, current effective object2 radius telemetry
    float visual0[4];  // merger blend, GW power, field gain OR source size, time since PN transition
    float visual1[4];  // plane overlay/detector distance, plane scale/source distance, remnant/system mass, remnant spin
    float reserved0[4]; // source size/r0, beam softness/E_rad, show mirrored source sphere, source sphere radius
    float reserved1[4]; // omega_QNM220, tau_QNM220, transition r, transition omega
};
static_assert(sizeof(BinaryFramePacket)==256, "BinaryFramePacket must be 256 bytes");
static_assert(alignof(BinaryFramePacket)==256, "BinaryFramePacket must be 256-byte aligned");
}

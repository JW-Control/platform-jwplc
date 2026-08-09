/*
 * JWPLC Basic v2.1.0-alpha.4 - P2 build-speed pilot.
 *
 * This core contains only one tiny translation unit so Arduino still runs its
 * normal core phase and produces build/core/core.a. platform.local.txt then
 * replaces that temporary archive with the board-specific precompiled core.a.
 *
 * The normal source core remains in cores/jwcontrol and is used to generate
 * the precompiled archive and as the public header include path.
 */

void jwplc_p2_core_stub(void)
{
}

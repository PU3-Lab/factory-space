# Pipe Empty Material Recipe

Target asset path:
- `/Game/FactorySpace/Materials/Pipe/M_Pipe_Empty`

Goal:
- Empty pipe should look like clear glass.
- It should read like clear glass without true translucent overlap artifacts.
- It should stay lightly tinted and readable against the floor.
- It should work with the parameters already prepared in `APipe`.

Recommended material settings:
- `Material Domain`: `Surface`
- `Blend Mode`: `Masked`
- `Shading Model`: `Default Lit`
- `Two Sided`: `false`
- `Opacity Mask Clip Value`: start around `0.2`

Recommended parameter names:
- `BaseColor` as `Vector Parameter`
- `Tint` as `Vector Parameter`
- `Opacity` as `Scalar Parameter`
- `Roughness` as `Scalar Parameter`
- `Specular` as `Scalar Parameter`
- `EmissiveStrength` as `Scalar Parameter`
- `EmissiveColor` as `Vector Parameter`

Recommended default values:
- `BaseColor`: `(0.85, 0.95, 1.0)`
- `Tint`: `(0.85, 0.95, 1.0)`
- `Opacity`: `0.18`
- `Roughness`: `0.12`
- `Specular`: `0.45`
- `EmissiveStrength`: `0.0`
- `EmissiveColor`: `(0.0, 0.0, 0.0)`

Simple node wiring:
- `BaseColor` output: multiply `BaseColor` and `Tint`, then connect to `Base Color`
- `Dithered mask`:
  - add a `DitherTemporalAA` node
  - feed `Opacity` into the `AlphaThreshold (S)` input of `DitherTemporalAA`
  - connect the result to `Opacity Mask`
- `Roughness` output: connect `Roughness` to `Roughness`
- `Specular` output: connect `Specular` to `Specular`
- `Emissive` output: multiply `EmissiveColor` and `EmissiveStrength`, then connect to `Emissive Color`

Optional glass polish:
- Add a `Fresnel` node
- Lerp between a darker center tint and brighter edge tint
- Multiply the Fresnel result lightly into `Base Color`
- Keep the effect subtle so the pipe still reads as transparent
- If the edge highlight gets too white, lower `Specular` first before raising `Roughness`

Why this helps:
- `Masked + DitherTemporalAA` avoids the repeated translucent stacking that made overlaps turn white
- The corner sphere and cylinder segments can still overlap visually, but they no longer accumulate semi-transparent shading in the same way
- This is also a better base for the later semi-transparent liquid color pass than `Alpha Composite` on the outer pipe shell

Code hookup:
- Assign `M_Pipe_Empty` to `APipe.EmptyPipeMaterial`
- Leave `FilledPipeMaterial` for the later flowing-liquid material
- Current `APipe` code will automatically use `EmptyPipeMaterial` when no liquid is present
- `APipe.EmptyPipeColor.A` currently drives the `Opacity` scalar parameter, so tune the apparent transparency from that alpha value or from the material instance

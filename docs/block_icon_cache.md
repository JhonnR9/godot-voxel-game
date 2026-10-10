# Block icon cache

The `BlockIconCache` autoload prepares icons before opening the menu. The first run generates 64 × 64 RGBA PNGs in `user://cache/block_icons`. Subsequent runs load these PNGs as `ImageTexture` resources shared by the creative inventory, hotbar, and item drag preview.

Composition uses a three-face projection matching `visual_bugs/images.png`, the generated registry's top and side textures, block tint, and per-face shading. Sampling uses nearest-neighbor filtering with a transparent background. Plants with the `crossed` flag retain their silhouette. Water uses its own color because its appearance in the world comes from a procedural shader.

Composition uses images from imported Texture2D resources, including in exported builds, without rendering a 3D scene or reading GPU pixels. With a valid cache, it does not load source images for composition.

`manifest.cfg` records a SHA-256 signature per block, combining the generator version, imported texture contents, and registry metadata. Missing, invalid, or incorrectly sized files are regenerated. Registry changes invalidate affected blocks; texture changes invalidate icons to prevent stale images. PNGs and the manifest are written to temporary files before replacement. If the disk is not writable, icons remain available in memory but will be generated again at the next startup.

The physical directory depends on the platform and Godot's `user://` configuration. Deleting this cache makes the game rebuild it on the next launch.

Test without changing the player's regular cache:

```sh
XDG_DATA_HOME=/tmp/godot-block-icon-tests godot --headless --path project --log-file /tmp/godot-icons-test.log --script res://tests/block_icon_cache_test.gd
```

The test checks initial generation, reuse with a new instance, recovery from missing or incorrectly sized files, tint changes, projection, and inventory integration. `last_run` exposes the `generated` and `loaded` counters to check independent runs.

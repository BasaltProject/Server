# Notice about `net.minecraft.*` and `com.mojang.*`

The Java sources under `java/net/minecraft/` and `java/com/mojang/` are an
**independent reimplementation** written for Basalt. They are not Mojang code.

- They exist so that plugins which expect the vanilla server classes can run on Basalt.
- Class, method and field names and signatures follow the published API, because compatibility
  with those plugins requires them to match.
- The method bodies are written from an understanding of vanilla behavior. They are not copied
  from Mojang source code, and no file from Mojang's game or server jar is included in this
  repository.
- `scripts/nms-manifest/` lists declarations only (signatures, no bodies), and
  `resources/stream_specs.txt` lists the field layout of network codecs, which are protocol facts.

Basalt is not affiliated with, endorsed by or sponsored by Mojang or Microsoft.

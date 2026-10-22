package net.minecraft.world.level.chunk;

import java.util.List;
import java.util.Optional;
import org.basaltmc.bukkit.Unsupported;
public interface PalettedContainerRO<T> {
    public default Object get(int x, int y, int z) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainerRO.get");
    }
    public default PalettedContainer recreate() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainerRO.recreate");
    }

    public record PackedData(List paletteEntries, Optional storage, int bitsPerEntry) {
        public PackedData(List list, Optional optional) {
            this(null, null, 0);
        }
    }
}

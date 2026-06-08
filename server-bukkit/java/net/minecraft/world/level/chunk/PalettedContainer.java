package net.minecraft.world.level.chunk;

import com.mojang.serialization.DataResult;
import org.basaltmc.bukkit.Unsupported;
public class PalettedContainer<T> implements PaletteResize, PalettedContainerRO {
    protected PalettedContainer() {
    }
    public Object get(int index) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainer.get");
    }
    public Object get(int x, int y, int z) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainer.get");
    }
    public Object getAndSetUnchecked(int x, int y, int z, Object value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainer.getAndSetUnchecked");
    }
    public void set(int x, int y, int z, Object value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainer.set");
    }
    public static DataResult unpack(Strategy strategy, PalettedContainerRO.PackedData packedData, Object objectValue, Object[] objects) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainer.unpack");
    }
}

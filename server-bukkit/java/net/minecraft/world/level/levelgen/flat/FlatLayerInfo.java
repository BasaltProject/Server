// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.flat;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class FlatLayerInfo {
    private final Holder<Block> block;
    private final int height;
    public static final Codec<FlatLayerInfo> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.flat.FlatLayerInfo.class, "CODEC");
    public FlatLayerInfo(int height, Block block) {
        this(height, block.builtInRegistryHolder());
    }
    public FlatLayerInfo(int height, Holder<Block> block) {
        this.height = height;
        this.block = block;
    }
    public int getHeight() {
        return height;
    }
    public BlockState getBlockState() {
        return block.value().defaultBlockState();
    }
    public FlatLayerInfo heightLimited(int maxHeight) {
        return height > maxHeight ? new FlatLayerInfo(maxHeight, block) : this;
    }
    public String toString() {
        return (height != 1 ? height + "*" : "") + block.getRegisteredName();
    }
}

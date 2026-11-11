// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.block.state.BlockState;
public interface PlainSignBlock {
    Attachment attachmentPoint(BlockState state);
    static Attachment getAttachmentPoint(BlockState blockState) {
        return blockState.getBlock() instanceof PlainSignBlock sign ? sign.attachmentPoint(blockState) : Attachment.GROUND;
    }
    public static enum Attachment implements StringRepresentable {
        WALL, GROUND;
        public static final Codec<Attachment> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.PlainSignBlock.Attachment.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}

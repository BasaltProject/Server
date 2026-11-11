// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.block.state.BlockState;
public interface HangingSignBlock {
    Attachment attachmentPoint(BlockState state);
    static Attachment getAttachmentPoint(BlockState blockState) {
        return blockState.getBlock() instanceof HangingSignBlock hanging ? hanging.attachmentPoint(blockState) : Attachment.CEILING;
    }
    public static enum Attachment implements StringRepresentable {
        WALL, CEILING, CEILING_MIDDLE;
        public static final Codec<Attachment> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.HangingSignBlock.Attachment.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}

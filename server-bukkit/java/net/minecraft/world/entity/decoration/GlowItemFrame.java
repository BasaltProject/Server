// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public class GlowItemFrame extends ItemFrame {
    public GlowItemFrame(EntityType<? extends ItemFrame> type, Level level) {
        super(type, level);
    }
    public GlowItemFrame(Level level, BlockPos pos, Direction direction) {
        super(level, pos, direction);
    }
    public SoundEvent getRemoveItemSound() {
        return null;
    }
    public SoundEvent getBreakSound() {
        return null;
    }
    public SoundEvent getPlaceSound() {
        return null;
    }
    public SoundEvent getAddItemSound() {
        return null;
    }
    public SoundEvent getRotateItemSound() {
        return null;
    }
    protected ItemStack getFrameItemStack() {
        return new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.GLOW_ITEM_FRAME);
    }
}
